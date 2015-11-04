
function strip_ext(name) {
  var l=name.length;
  if(l>4) {
    var len=l-4;
    var ext=name.substring(len);
    if(ext!=null&&(ext=='.mp4'||ext=='.mkv'||ext=='.sdp')) {
      var len=name.length-4;
      name=name.substring(0, len);
    }
  }
  return name;
}

function truebody(doc){
  return (!window.opera && doc.compatMode && doc.compatMode!="BackCompat")? doc.documentElement : doc.body
}

function getfileinfoshareobj(){
  if(document.getElementById) {
    var obj = document.getElementById("fileinfoshareid");
    if(obj == null) 
      obj = parent.document.getElementById("fileinfoshareid");
    if(obj == null) 
      obj = document.getElementById("fileinfoshareid2");
    return obj;
  } else if(document.all)
    return document.all.fileinfoshareid;
}
function hidesharelink() {
  getfileinfoshareobj().style.visibility='hidden';
}

function sharelink() {
  var obj=getfileinfoshareobj();
  var hvrtxt='<div style="padding: 4px; border: 10px; background-color: #cccccc;" align="center" width="100%" height="100%">';
  hvrtxt+='<div class="bg2" align="right" width="100%"><a onclick="javascript:hidesharelink();" href="#"><img src="/img/close2.png" alt="Close" border="0"></a></div>';
  hvrtxt+='<div class="bg2" width="100%">';

  hvrtxt+='test<br>test<br>test3<br>a';

  hvrtxt+='</div></div>';

  wid = 500;
  var posx=(document.all? truebody().scrollLeft+truebody().clientWidth : pageXOffset+window.innerWidth -wid)/2;
  var ht=document.all? Math.min(truebody().scrollHeight, truebody().clientHeight) : Math.min(window.innerHeight) - 100;

  posy=250;

  obj.innerHTML=hvrtxt;
  obj.style.width=wid+"px";
  obj.style.height=ht+"px";
  obj.style.visibility="visible";
  obj.style.left=posx+"px";
  obj.style.top=posy+"px";


/*
  wnd = open("", "hoverwindow", "width=300,height=200,left=10,top=10");
  wnd.document.open();
  wnd.document.write("<html><body>");
  wnd.document.write("test");
  wnd.document.write("</body></html>");
  wnd.document.clse();
*/
}

function cbMPInfo(id, txt) {
  
  if(id == 3) {

    var strinfo='';
    var needbr=0;
    var retParams=getRespArray(txt);

    var _vc=retParams['vc'];
    var _ac=retParams['ac'];
    var _dur=retParams['dur'];
    var _sz=retParams['sz'];
    var _tm=retParams['tm'];
    var _dispname=retParams['n'];
    var _islive=0;
    var file=retParams['file'];

    if(_dispname==null) {
      _dispname=file;
    }

    var strprof='';
    var idx=0;
    var havenondefault=0;

    var strname='<br>';

    strname+='Showing <i>'+unescape(_dispname)+'</i>&nbsp;&nbsp;&nbsp;'

    //strname+='<a target="_top" href="'+location.protocol+'//'+location.host+'/'+strip_ext(file)+'">';
    strname+='<a target="_top" href="'+location.protocol+'//'+location.host+'/'+file+'">';
    strname+='<b>Link to this video</b>';
    strname+='</a>';
    //strname+=' <a href="#" onclick="javascript:sharelink();">Share this video</a>';

    do {
      _prof=retParams['prof_'+idx];
      if(_prof!=null) havenondefault=1;
      idx++;
      strprof+='<a href="/live/'+file;
      if(_prof!=null) strprof+='/prof_'+(_prof!=null?_prof:'');
      strprof+='?nologo=1';
      strprof+='">'+(_prof!=null?_prof:'default')+'</a>';
      strprof+='&nbsp;&nbsp;';

    } while(_prof!=null);

    if(!havenondefault) strprof='';

    if(file!=null && file.length>4 && file.substr(file.length-4)==".sdp") {
      _islive=1;
    }

    if(!_islive) { 
      strinfo+='<table border="0" cellpadding="0" cellspacing="0"><tr><td>';
      if(_dur!=null&&_dur>0) {
        strinfo+='Duration &nbsp;'+formatduration(_dur*1000)+'&nbsp;&nbsp;';
        needbr=1;
      }
      if(_sz!=null) {
         strinfo+='&nbsp;'+formatsize(_sz);
        needbr=1;
      }
      if(_dur>0&&_sz>0) {
        var _kbps=_sz/128/_dur;
        needbr=1;
        strinfo+='&nbsp;&nbsp;';
        if(_kbps<1000)
          strinfo+=Math.round(_kbps*10)/10+' Kb/s';
        else if(_kbps)
          strinfo+=Math.round(_kbps/1024*10)/10+' Mb/s';
      }

      if(_tm!=null&&_tm.length>1) {

        var dt=new Date();
        _tm=parseInt(_tm);
        dt.setTime(_tm * 1000);
        var _day=dt.getDate();
        if(++_day<10) _day='0'+_day;
        var _mon=dt.getMonth();
        if(++_mon<10) _mon='0'+_mon;
        var _yr=dt.getYear();
        _yr-=100;
        if(_yr<10) _yr='0'+_yr;
        var _hr=dt.getHours();
        if(_hr<10) _hr='0'+_hr;
        var _min=dt.getMinutes();
        if(_min<10) _min='0'+_min;

        //strinfo+='&nbsp;&nbsp;Created on '+_hr+':'+_min+' '+_mon+'/'+_day+'/'+_yr;
      }

      if((_vc!=null&&_vc!=' ') || (_ac!=null&&_ac!=' ')) {
        strinfo+='<br>';
        if(_vc!=null&&_vc!=' ') {
          strinfo+=_vc;
        }
        if(_ac!=null&&_ac!=' ') {
          strinfo+=_ac;
        }
      }

      strinfo+='</td></tr></table>';
    }

    var divfileinfo=document.getElementById('fileinfo');
    var divfileinfo2=document.getElementById('fileinfo2');
    var divfileinfo3=document.getElementById('fileinfo3');

    if(divfileinfo!=null)
      divfileinfo.innerHTML=strname;
    if(divfileinfo2!=null)
      divfileinfo2.innerHTML=strprof;
    if(divfileinfo3!=null)
      divfileinfo3.innerHTML=strinfo;
  }
}

function setDimensions(x, y) {
  var maxx=truebody(document).clientWidth-40;
  var maxy=truebody(document).clientHeight-70;

  if(xres>0&&yres>0&&x>0&&y==undefined) {
    var ar=yres/xres;
    xres=x;
    yres=Math.round(x*ar);
  } else if(xres>0&&yres>0&&x==undefined&&y>0) {
    var ar=xres/yres;
    xres=Math.round(y*ar);
    yres=y;
  } else {
    if(x>0)xres=x;
    if(y>0)yres=y;
  }

  if(maxy>0&&yres>maxy) {
    var ar=maxy/yres;
    xres*=ar;
    yres=maxy;
  }
    
  if(maxx>0&&xres>maxx) {
    var ar=maxx/xres;
    yres*=ar;
    xres=maxx;
  }

}

var livePollReq=null;
var livePollTimer=null;
var livePollTm=0;
var redirId=null;

function cbLivePollSuccess(id, txt) {

  if(id == 1) {
    var dt=new Date().getTime();
    livePollTm=dt;
    //var obj = document.getElementById("fileinfo3");
    //if(null!=obj) {
    //  obj.innerHTML='config response now ' + dt;
    //}
  }

}
/*
function cbLivePollError(id, code) {
  if(id == 1) {
    var obj = document.getElementById("fileinfo3");
    if(null!=obj) {
      var dt=new Date().getTime();
      obj.innerHTML='ERROR code:'+code+ ', config response now ' + dt;
    }
  }
}
*/

function livepollredirector() {

  var retParams=getRespArray(window.location.href);
  redirId=retParams['id'];

  if(redirId==null) {
    return;
  }

  var dt=new Date().getTime();
  if(livePollTm==0)
    livePollTm=dt;
  else {
    var sec=(dt-livePollTm)/1000;
    if(sec>5) {
      var obj = document.getElementById("fileinfo2");
      var port=8090;
      var url='/webrtc-demo/x/l/'+redirId;
      var strhref=window.location.protocol+'//'+window.location.hostname+':'+port+url;

      if(obj!=null) {
        var txt='The live stream has ended.<br>';
        txt+='View the recording. &nbsp; <a href="'+strhref+'">'+strhref+'</a>';
        obj.innerHTML=txt;
      }

      if(sec>11) {
        window.location=strhref;     
      }
    }
    
  }

  if(null==livePollReq) 
    livePollReq=createAjax();
  ajaxGet(livePollReq, '/config?checklive=1', cbLivePollSuccess, null, 1);

  clearTimeout(livePollTimer);
  livePollTimer=setTimeout("livepollredirector();", 4000);
}

