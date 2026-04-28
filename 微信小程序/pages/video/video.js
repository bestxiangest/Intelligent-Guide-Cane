const appConfig = require('../../config')

const STORAGE_KEY = 'guideCaneCameraAddress'

function normalizeCameraHost(value) {
  let host = String(value || '').trim()
  host = host.replace(/^https?:\/\//i, '')
  host = host.split('?')[0]
  host = host.split('#')[0]
  host = host.split('/')[0]
  host = host.split(':')[0]
  return host
}

function formatTime(date) {
  const pad = (num) => String(num).padStart(2, '0')
  return `${pad(date.getHours())}:${pad(date.getMinutes())}`
}

Page({
  data: {
    addressInput: '',
    deviceAddress: '',
    healthUrl: '',
    streamUrl: '',
    streamAltUrl: '',
    activePreviewUrl: '',
    streamMode: '81',
    statusText: '未连接',
    statusTone: 'idle',
    viewNote: '等待导盲杖画面',
    lastSeenText: '',
    canPreview: false,
    isConnecting: false,
    quickHosts: []
  },

  onLoad(options) {
    const optionAddress = normalizeCameraHost((options || {}).ip)
    const storedAddress = normalizeCameraHost(wx.getStorageSync(STORAGE_KEY))
    const hotspotAddress = normalizeCameraHost(appConfig.defaultCameraIp)
    const directAddress = normalizeCameraHost(appConfig.directCameraIp)
    const deviceAddress = optionAddress || storedAddress || hotspotAddress
    const quickHosts = [
      { label: '热点地址', value: hotspotAddress },
      { label: '设备直连', value: directAddress }
    ].filter((item) => item.value)

    this.setData({
      quickHosts,
      addressInput: deviceAddress,
      deviceAddress
    })

    if (deviceAddress) {
      this.rebuildUrls(deviceAddress)
      this.setStatus('待确认', 'idle')
      this.checkDevice(deviceAddress)
    }
  },

  setStatus(text, tone) {
    this.setData({
      statusText: text,
      statusTone: tone || 'idle'
    })
  },

  buildUrls(deviceAddress) {
    const nonce = Date.now()
    return {
      healthUrl: `http://${deviceAddress}/health?t=${nonce}`,
      streamUrl: `http://${deviceAddress}:81/stream?t=${nonce}`,
      streamAltUrl: `http://${deviceAddress}/stream?t=${nonce}`
    }
  },

  getActivePreviewUrl(urls, streamMode) {
    return streamMode === '80' ? urls.streamAltUrl : urls.streamUrl
  },

  rebuildUrls(deviceAddress, streamMode) {
    const urls = this.buildUrls(deviceAddress)
    const nextStreamMode = streamMode || this.data.streamMode
    this.setData({
      deviceAddress,
      healthUrl: urls.healthUrl,
      streamUrl: urls.streamUrl,
      streamAltUrl: urls.streamAltUrl,
      activePreviewUrl: this.getActivePreviewUrl(urls, nextStreamMode),
      canPreview: Boolean(deviceAddress)
    })
    return urls
  },

  onAddressInput(e) {
    this.setData({
      addressInput: e.detail.value
    })
  },

  useQuickHost(e) {
    const deviceAddress = normalizeCameraHost(e.currentTarget.dataset.ip)
    if (!deviceAddress) return

    this.setData({
      addressInput: deviceAddress
    })
    this.connectDevice(deviceAddress)
  },

  connectDevice(addressValue) {
    const rawAddress = typeof addressValue === 'string' ? addressValue : this.data.addressInput
    const deviceAddress = normalizeCameraHost(rawAddress)
    if (!deviceAddress) {
      wx.showToast({
        title: '请输入设备地址',
        icon: 'none'
      })
      return
    }

    wx.setStorageSync(STORAGE_KEY, deviceAddress)
    this.setData({
      streamMode: '81'
    })
    this.rebuildUrls(deviceAddress, '81')
    this.checkDevice(deviceAddress)
  },

  checkDevice(addressValue) {
    const rawAddress = typeof addressValue === 'string'
      ? addressValue
      : this.data.deviceAddress || this.data.addressInput
    const deviceAddress = normalizeCameraHost(rawAddress)
    if (!deviceAddress) {
      wx.showToast({
        title: '请先连接设备',
        icon: 'none'
      })
      return
    }

    const urls = this.rebuildUrls(deviceAddress)
    this.setData({ isConnecting: true })
    this.setStatus('连接中', 'testing')

    wx.request({
      url: urls.healthUrl,
      method: 'GET',
      timeout: 5000,
      success: (res) => {
        if (res.statusCode === 200 && res.data) {
          this.setData({
            viewNote: '导盲杖画面已就绪',
            lastSeenText: formatTime(new Date())
          })
          this.setStatus('在线', 'ok')
        } else {
          this.setData({
            viewNote: '设备有响应，画面暂未确认',
            lastSeenText: formatTime(new Date())
          })
          this.setStatus('待确认', 'warn')
        }
      },
      fail: () => {
        this.setData({
          viewNote: '请确认手机与导盲杖在同一网络',
          lastSeenText: formatTime(new Date())
        })
        this.setStatus('未连接', 'bad')
      },
      complete: () => {
        this.setData({ isConnecting: false })
      }
    })
  },

  refreshPreview() {
    if (!this.data.deviceAddress) {
      wx.showToast({
        title: '请先连接设备',
        icon: 'none'
      })
      return
    }

    this.rebuildUrls(this.data.deviceAddress, this.data.streamMode)
    this.setData({
      viewNote: '画面已刷新',
      lastSeenText: formatTime(new Date())
    })
  },

  tryBackupStream() {
    if (!this.data.deviceAddress || this.data.streamMode === '80') {
      return false
    }

    this.setData({
      streamMode: '80',
      viewNote: '正在尝试备用连接'
    })
    this.rebuildUrls(this.data.deviceAddress, '80')
    return true
  },

  onPreviewLoad() {
    this.setData({
      viewNote: '导盲杖画面正在同步',
      lastSeenText: formatTime(new Date())
    })
    if (this.data.statusTone !== 'ok') {
      this.setStatus('在线', 'ok')
    }
  },

  onPreviewError() {
    if (this.tryBackupStream()) {
      this.setStatus('连接中', 'testing')
      return
    }

    this.setData({
      viewNote: '画面暂时不可用',
      lastSeenText: formatTime(new Date())
    })
    this.setStatus('未连接', 'bad')
  }
})
