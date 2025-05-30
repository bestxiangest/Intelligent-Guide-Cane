
var amapFile = require('../../libs/amap-wx.130');
var myAmapFun = new amapFile.AMapWX({key:'615843fbd37181bd0b57ab57279920fa'});
Page({
  data: {
    markers: [],
    latitude: '',
    longitude: '',
    textData: {},
    destination:{
        id:1,
        name:'华东交通大学',
        longitude:115.868517,
        latitude:28.742945,
        iconPath:'../../images/des.png',
        width:35,
        height:35
    },
    myPosition:{},
    scale:17,
    polyline:[],
    points:[],
    blind_stick:{
        id :2,
        name:'导盲杖',
        longitude:115.868517,
        latitude:28.742945,
        name:'导盲杖',
        iconPath:'../../images/stick.png',
        width:30,
        height:30
    },
    steps:[],
    navigating:false,
    traffic_lights:0,
    cnt:0
  }, 
  //调整地图缩放以展示所有的点
  adjustMapView() {
    const points = this.data.markers.map(marker => ({
      latitude: marker.latitude,
      longitude: marker.longitude
    }))
    points.push({longitude:this.data.myPosition.longitude,latitude:this.data.myPosition.latitude})
    this.setData({
        points:points
    })
  },
  //逆地址解析
  reverseGps(){
    var that=this
    myAmapFun.getRegeo({
        location:''+that.data.longitude+','+that.data.latitude,
        success:function(data){
            that.setData({
                textData:{
                    name:data[0].name,
                    desc:data[0].desc
                }
            })
        },
        fail: function(info){
          wx.showModal({title:info.errMsg})
        }
    })
  },
  //点击地图
  makertap: function(e) {
      var that =this
      const id= e.detail.markerId
      if(id){
          //点击已有标记点
          var mark =that.data.markers[id]
          if(that.data.navigating){
            mark.iconPath='../../images/point.png'
            that.setData({
                markers:that.data.markers.map(m => m.id===(id)? mark:m)
            })
          }else{
            mark.iconPath='../../images/des.png'
            that.setData({
                markers:that.data.markers.map(m => m.id===(id)? mark:m),
                textData:{
                    name:mark.name,
                    desc:''
                },
                destination:mark
            })
          }
          return ;
      }
      var mark={
          id:that.data.cnt++,
          longitude:e.detail.longitude,
          latitude:e.detail.latitude,
          iconPath:'../../images/des.png',
          width:30,
          height:30
      }
      if(that.data.navigating){
        mark.iconPath='../../images/point.png'
        that.setData({
            markers:[that.data.destination,mark]
        })
      }else{
        mark.iconPath='../../images/des.png'
        myAmapFun.getRegeo({
            location:''+mark.longitude+','+mark.latitude ,
            success:function(data){
                that.setData({
                    textData:{
                        name:data[0].name,
                        desc:data[0].desc
                    },
                    markers: [mark],
                    destination:mark
                })
            },
            fail: function(info){
              wx.showModal({title:info.errMsg})
            }
        })
      }
  },

  onLoad: function() {
      var that=this
      wx:wx.request({
        url: 'http://60.215.128.117:64076',
        method:"GET",
        success: (result) => {
            that.setData({
                blind_stick:{
                    id:2,
                    longitude:result.data.split(',')[0],
                    latitude:result.data.split(',')[1],
                    name:'导盲杖',
                    iconPath:'../../images/stick.png',
                       width:30,
                       height:30
                }
            })
        },
        fail: (err) => {
            wx.showModal({title:err.info,})
        },
      })
      that.toMyposition()
  },
  to_blind(){
      var that = this
        wx:wx.request({
        url: 'http://60.215.128.117:64076',
        method:"GET",
        success: (result) => {
            that.setData({
                blind_stick:{
                    id:2,
                    longitude:result.data.split(',')[0],
                    latitude:result.data.split(',')[1],
                    name:'导盲杖',
                    iconPath:'../../images/stick.png',
                       width:30,
                       height:30
                }
            })
        },
        fail: (err) => {
            wx.showModal({title:err.info,})
        },
      })
    
    this.setData({
        markers:[this.data.blind_stick]
    })
  },
  toMyposition(){
      var that =this
      wx.getLocation({
          type:'wgs84',
          success:function(res){
              var pos ={
                  latitude:res.latitude,
                  longitude:res.longitude,
                  id:0,
                  name:'我的位置'
              }
              that.setData({
                  latitude:res.latitude,
                  longitude:res.longitude,
                  markers:[],
                  myPosition:pos,
                  points:[{
                      longitude:res.longitude,
                      latitude:res.latitude
                  }]
              })
              that.reverseGps()
          },
          fail: function(info){
            wx.showModal({title:info.errMsg})
          }
      })
  },
  inputAddress: function(e){
      var that= this
      var value =e.detail.value
      if(value=='')return;
      myAmapFun.getInputtips({
          keywords:value,
          success:function(data){
            //   console.log(data)
              if(data.tips.length>0){
                  var list =data.tips.map((item,index)=>{
                      return{
                          id:index,
                          name:item.district+item.name,
                          longitude:item.location.split(',')[0],
                          latitude:item.location.split(',')[1],
                          iconPath:'../../images/point.png',
                          joinCluster:true,
                          width:30,
                          height:30
                      }
                  })
                  that.setData({
                      markers:list
                  })
              }
            that.adjustMapView()
          },
          fail:function(info){
            wx.showModal({title:info.errMsg})
          }
      })
  },
  navigation(){
      if(this.data.navigating){
          this.setData({
              navigating:false,
              polyline:[],
              textData:{
                  name: this.data.myPosition.name,
                  desc:''
              }
          })
          return;
      }else{
          this.setData({
              navigating:true
          })
      }
      const that =this
      const {myPosition,destination} =this.data
      if(!myPosition.latitude||!myPosition.longitude||!destination.latitude||!destination.longitude){
          wx.showModal({title: '缺少目的地或起始地点信息'})
          return;
      }
    myAmapFun.getDrivingRoute({ 
        origin: `${myPosition.longitude},${myPosition.latitude}`,
        destination: `${destination.longitude},${destination.latitude}`,
        success: function(data) {
          console.log(data)
          if (data.paths && data.paths.length > 0) {
            const points = [];
            that.setData({
                steps:data.paths[0].steps,
                traffic_lights:data.paths[0].traffic_lights
            })
            data.paths[0].steps.forEach(step => {
              step.polyline.split(';').forEach(point => {
                const [lng, lat] = point.split(',');
                points.push({ longitude: parseFloat(lng), latitude: parseFloat(lat) });
              });
            });
            that.setData({ 
                polyline: [{ points, color: '#46adf9', width: 6, opacity: 0.8 }],
                markers:[that.data.destination]
            })
            that.adjustMapView()
            that.showNavigation()
          } else {
            wx.showModal({ title: '未找到合适的驾车路线' });
          }
        },
        fail: function(info) {wx.showModal({ title: info.errMsg })}
      })
  },
  toSelected : function(e){
    //   console.log(e)
      var position = e._relatedInfo.anchorTargetText
      const mark =this.data.markers
      for(let i=0;i<mark.length;i++){
          mark[i].iconPath='../../images/point.png'
          if(mark[i].name === position){
              mark[i].iconPath='../../images/des.png'
              mark[i].width=40
              mark[i].height=40
              this.setData({
                  latitude :mark[i].latitude,
                  longitude:mark[i].longitude,
                  scale:17,
                  textData:{
                      name:mark[i].name,
                      desc:mark[i].desc
                  },
                  destination:{
                    id:1,
                    name:mark[i].name,
                    longitude:mark[i].longitude,
                    latitude:mark[i].latitude,
                    iconPath:'../../images/des.png',
                    width:35,
                    height:35
                  }
              })
          }
      }
      this.setData({
          markers:mark
      })
  },
  showNavigation(){
      let total_time = 0
      let total_distance = 0
      const step =this.data.steps
      step.forEach(step =>{
          total_distance += Number(step.distance)
          total_time+=Number(step.duration)
      })
      let distanceStr
      if(total_distance>1000){
         distanceStr= (total_distance/1000).toFixed(2)+'km'
      }else{
          distanceStr = total_distance+'m'
      }

      let hours = Math.floor(total_time / 3600);
      let minutes = Math.floor((total_time % 3600) / 60);
      let seconds = total_time % 60;
      let timeStr = '';
      if (hours > 0) {
          timeStr += hours + ' 小时';
      }
      if (minutes > 0) {
          timeStr += minutes + ' 分钟';
      }
      if (seconds > 0) {
          timeStr += seconds + ' 秒';
      }
      let info = '距离目的地共' + distanceStr + '，预计用时' + timeStr;
      let lights = `沿途共有 ${this.data.traffic_lights} 个红绿灯`;
      this.setData({
          textData: {
              name: info,
              desc: lights
          }
      });
  }
});
