

function getRespArray(uri) {
  retParams=new Array();
  var pos=uri.indexOf('?');
  if(pos>0) 
    uri=uri.substr(pos+1);
  var pairs=uri.split('&');
  for(var i=0; i < pairs.length; i++) {
    var pos=pairs[i].indexOf('=');
    if(pos==-1) continue;
    var key=pairs[i].substring(0,pos);
    var val=pairs[i].substring(pos+1);
    retParams[key]=val;
  }
  return retParams;
}

function createAjax() {
  // Mozilla/Safari
  if (window.XMLHttpRequest) {
    req=new XMLHttpRequest();
  }
  else if (window.ActiveXObject) {
  // IE
    req=new ActiveXObject("Microsoft.XMLHTTP");
  }
  return req;
}

function ajaxGet(req, reqStr, cbSuccess, cbError, id) {

  var sendasync=true;
  req.open('GET', reqStr, sendasync);

  //req.onreadystatechange=function() {
  req.onload=function() {  // firefox needs onload

    var objcmdresp=document.getElementById('cmdresp');
    if(objcmdresp==null)
      objcmdresp=parent.document.getElementById('cmdresp');

    if(objcmdresp!=null) objcmdresp.innerHTML='';
    //alert('ajaxGet...readyState:'+ req.readyState);
    if(req.readyState==4) {

      if(req.status==0) {
        if(cbError!=null)
          cbError(id, -1); 
        else if(objcmdresp!=null) 
          objcmdresp.innerHTML='<div class=\"cstatusred\">Server Unavailable&nbsp;</div>';
      } else if(req.status==200) {
        if(objcmdresp!=null) 
          objcmdresp.innerHTML='&nbsp;';
        cbSuccess(id, req.responseText);
      } else {
        if(cbError!=null)
          cbError(id, req.status); 
        else if(objcmdresp!=null) { 
          var str;
          if(req.status==403) str='Forbidden';
          else if(req.status==404) str='Not Found';
          else if(req.status>=500) str='Server Error';
          else str='Server returned code '+req.status;
          objcmdresp.innerHTML='<div class=\"cstatusred\">'+str+'&nbsp;</div>';
        }
      }
    }
  };


  //if (window.XMLHttpRequest) {
  //  req.send(null);
  //} else {
    req.send();
  //}

  if(!sendasync) {
    req.onreadystatechange();
  }

}

function formatsize(sz) {
  return Math.round(sz/1048576*10)/10+' MB';
}


function formatduration(msin) {
  var sec=msin/1000;
  var hr=Math.floor(sec/3600);
  var min=Math.floor(sec/60)%60;
  var sec2=Math.floor(sec%60);
  var ms='';
  var str='';

  if(hr>0)
    str+=hr+':';
  if(hr>0||min>0) {
    if(hr>0&&min<10)
      str+='0';
    str+=min+':';
  }
  if(sec2<10)
    str+='0';
  str+=sec2;
  if(msin<60000) {
    ms=Math.round((ms%10000)/1000);
    str+='.'+ms;
  }

  if(sec<60)
    str+=' sec';

  return str;
}

function truebody(doc){
  return (!window.opera && doc.compatMode && doc.compatMode!="BackCompat")? doc.documentElement : doc.body
}

function getRsrcNameFromUri(uri) {

  var state=0;
  var last=1;
  var pos=uri.length;
  var rsrc=null;

  for(i=1;i<pos;i++) {
    if(state==0&&uri[i]=='/' && uri[i-1]=='/')
      state=1;
    else if(state>0&&uri[i]=='/' && uri[i-1]!='/') {
      state++;
       last=i;
  
      if(state==2) {
        var p=uri.substr(last+1, 10);
        if(p.substr(0, 4) != "live" &&
           p.substr(0, 4) != "rtmp" &&
           p.substr(0, 3) != "flv" &&
           p.substr(0, 8) != "httplive" &&
           p.substr(0, 4) != "rtsp") {
          break;
        }
      }
    }
    if(state>=3) {
      break;
    }
  }

  if(state>=2) {
    uri=uri.substr(last+1);
  }

  pos=uri.indexOf('?');
  if(pos<0)
    pos = uri.length;

  for(n=0;n<2;n++) {
    for(idx=pos;idx>0;idx--) {
      if(uri[idx]=='/') {
        rsrc=uri.substr((idx+1), (pos-idx));  
        if(rsrc[rsrc.length-1]=='?') rsrc=rsrc.substr(0,rsrc.length-1);
        break;
      }
    }
    if(rsrc!=null&&rsrc.substr(0,5) == "prof_") {
      profile=rsrc.substr(5);
      rsrc=null;
      idx--;
      pos = idx+1;
    } else {
      break;
    }
  }

  rsrc=uri.substr(0, pos);

  return rsrc;
} 

function appendUriParam(uri, param) {
  if(uri!=null) {
    if(uri.indexOf('?') < 0)
      uri+='?'
    else if(uri[uri.length-1]!='&') 
      uri+='&'
    uri+=param;
  }
  return uri;
}

function loadjs(path) {
  var fileref=document.createElement('script')
  fileref.setAttribute("type","text/javascript")
  fileref.setAttribute("src", path)

  if (typeof fileref!="undefined")
    document.getElementsByTagName("head")[0].appendChild(fileref);
}
