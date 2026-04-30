const appConfig = require('../../config')

const STORAGE_KEY = 'guideCaneCameraAddress'
const STREAM_PORT = 81
const STREAM_PATH = '/stream'

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
    streamUrl: '',
    activePreviewUrl: '',
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
      this.rebuildStream(deviceAddress)
      this.setStatus('待确认', 'idle')
    }
  },

  setStatus(text, tone) {
    this.setData({
      statusText: text,
      statusTone: tone || 'idle'
    })
  },

  buildStreamUrl(deviceAddress) {
    return `http://${deviceAddress}:${STREAM_PORT}${STREAM_PATH}`
  },

  rebuildStream(deviceAddress) {
    const streamUrl = this.buildStreamUrl(deviceAddress)
    this.setData({
      deviceAddress,
      streamUrl,
      activePreviewUrl: '',
      canPreview: false
    })
    if (deviceAddress) {
      setTimeout(() => {
        if (this.data.deviceAddress !== deviceAddress || this.data.streamUrl !== streamUrl) return
        this.setData({
          activePreviewUrl: streamUrl,
          canPreview: true
        })
      }, 30)
    }
    return streamUrl
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
      isConnecting: false,
      viewNote: '正在打开导盲杖画面',
      lastSeenText: formatTime(new Date())
    })
    this.rebuildStream(deviceAddress)
    this.setStatus('连接中', 'testing')
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

    this.rebuildStream(deviceAddress)
    this.setData({
      isConnecting: false,
      viewNote: '已重新打开热点视频流',
      lastSeenText: formatTime(new Date())
    })
    this.setStatus('连接中', 'testing')
  },

  refreshPreview() {
    if (!this.data.deviceAddress) {
      wx.showToast({
        title: '请先连接设备',
        icon: 'none'
      })
      return
    }

    this.rebuildStream(this.data.deviceAddress)
    this.setData({
      viewNote: '画面已刷新',
      lastSeenText: formatTime(new Date())
    })
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
    this.setData({
      viewNote: '画面暂时不可用',
      lastSeenText: formatTime(new Date())
    })
    this.setStatus('未连接', 'bad')
  }
})
