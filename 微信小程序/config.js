const defaults = {
  amapKey: '8682f9b2b4e2fc0a58ca5515afa91e9f',
  defaultCameraIp: '10.121.190.19',
  directCameraIp: '192.168.4.1',
  defaultDestination: {
    name: '电子科技大学中山校区',
    longitude: 113.390342,
    latitude: 22.527403
  },
  defaultBlindStick: {
    name: '导盲杖',
    longitude: 120.077711,
    latitude: 30.305955
  }
}

let localConfig = {}

try {
  localConfig = require('./config.local')
} catch (error) {
  localConfig = {}
}

module.exports = Object.assign({}, defaults, localConfig)
