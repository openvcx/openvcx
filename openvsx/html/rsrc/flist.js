

function frameprfx() {
  frm=top.frames['listframe'];
  if(frm!=null)
    return 'top.frames[\'listframe\'].';
  else return '';
}

function truncname(name, max, brk) {
  var _name=name;

  if(brk>0) {
    _name='';
    var idxprev=0;
    var havesp=0;
    for(idx=0;idx<name.length;idx++) {
      if(name.charAt(idx)==' ') havesp=1;
      if(idx>0&&idx%brk==0) {
        var _end=brk+idxprev;
        _name+=name.substring(idxprev,_end);
        if(havesp==0) _name+=' ';
        idxprev=idx;
        havesp=0;
      }
    }
    if(idx>idxprev) {
      _name+=name.substring(idxprev, idx);
    }
  }

  var len=_name.length;
  if(len>max) {
    var nameshort=_name.substring(0,max-7); 
    nameshort+='... '; 
    nameshort+=_name.substring(len-3);
    return nameshort;
  }

  return _name;
}

function doubledelims(path) {
  var pairs=path.split('\\');
  var str='';

  for(var i=0; i < pairs.length; i++) {
    if(i>0) {
      str+='\\\\';
    }
    str+=pairs[i];
  }
  return str;
}

function getparentdir(dir, c) {
  var parentdir='';

  for(i=dir.length;i>0;i--) {
    if(dir.charAt(i)==c) {
      parentdir=dir.substring(0,i);
      break;
    }
  }

  return parentdir;
}

function dispdirstr(dir) {
  var pairs=dir.split('\\');
  var dirname='';
  var str='';
 
  if(pairs.length > 0 && pairs[0].length > 0) {
    str='<a onclick="javascript:'+frameprfx()+'listdir(\'\');" href="#">..</a>';
  } else {
    str='';
  }

  for(var i=0; i < pairs.length; i++) {
    if(pairs[i].length > 0) {
      str+='\\';
    }
    if(i < pairs.length -1) {
      str+='<a onclick="javascript:'+frameprfx()+'listdir(\''+doubledelims(dirname+pairs[i])+'\');" href="#">'+pairs[i]+'</a>';
      dirname+=pairs[i];
      dirname+='\\';
    } else {
      str+='<a onclick="javascript:'+frameprfx()+'listdir(\''+doubledelims(dirname+pairs[i])+'\');" href="#">'+pairs[i]+'</a>';
    }
  }
  return str; 
}

function urlizestr(dir) {
  var str='';
  var pairs=dir.split('\\');
  for(var i=0; i<pairs.length; i++) {
    str+=pairs[i];
    if(i<pairs.length-1) {
      str+='/'; 
    }
  }
  return str;
}

function drawfilediv(dir, retParams, i) {
  var str='';
  var prmf=retParams['f_'+i];
  var prmd=retParams['d_'+i];
  var prmtns=retParams['t_'+i];
  var prmtm=retParams['c_'+i];
  var prmn=retParams['n_'+i];
  var _curfile=unescape(curplayfile);
  
  if(prmf!=null) {

    if(prmn==null) {
      prmn=prmf;
    }

    var fullpath=escape(dir);
    if(prmd!=null&&prmd.charAt(0)!='/'&& fullpath!=null&&fullpath.length>0&&fullpath.charAt(fullpath.length-1)!='/')
      fullpath+='/';
    fullpath+=prmf;

    if(prmtns==null) prmtns=0;
    else if(prmtns >0) prmtns--;

    var stylestr='';
    if(curplayfile!='' && unescape(fullpath)==_curfile) {
      stylestr+='background-color: #cccccc;'; 
    }

    str+='<div class="thumbouter" style="width:'+colwid+'px;'+stylestr+'"';
    if(!ipod)
      str+=' onmouseover="this.className=\'thumbouterhvr\'" onmouseout="this.className=\'thumbouter\'"';
    str+=' id="fo_'+i+'">';

    if(prmtns>0) {
      str+='<a style="position:relative;cursor:hand;" name="'+fullpath+'" onclick="javascript:playfile2(\''+urlizestr(fullpath)+'\',\'fl_'+i+'\');">';
      str+='<div class="vidlink" style="background:url(\'tmn/'+fullpath+'_xn.jpg\') no-repeat center;"';
      if(!ipod) 
        str+=' onmouseover="showxtrainfo2(\''+'fl_'+i+'\',\''+doubledelims(fullpath)+'\','+prmtns+');" onmouseout="hidextrainfo2();"';
      str+=' id="fl_'+i+'">';
      str+='</div>';
      str+='</a>';

    } else {
      str+='<div class="novidlink"';
      str+=' id="fl_'+i+'"><a name="'+fullpath+'" onclick="javascript:playfile2(\''+urlizestr(fullpath)+'\',\'fl_'+i+'\');"';
      str+=' href="#"><img class="vidplaybluebigbtn2" src="img/playbluebig.png" alt="Play"';
      str+='></a> ';
      str+='</div>';
    }

    str+='<br>';

    str+='<a class="flink" href="#" onclick="javascript:playfile2(\''+urlizestr(fullpath)+'\',\'fl_'+i+'\');"';
    if(uipwd!='') str+='?pass='+uipwd;
    str+='\"/>';
    str+=truncname(unescape(prmn),26);
    str+='</a>';
         
    if(prmd && prmd>0) {
      str+='<br>&nbsp;&nbsp;&nbsp;&nbsp;'+formatduration(prmd*1000);
    } else str+='<br>&nbsp;';

    str+='</div>';

  } else if(prmd) {

    var fullpath=dir;
 
    if(prmd!=null&&prmd.charAt(0)!='/'&& fullpath!=null&&fullpath.length>0&&fullpath.charAt(fullpath.length-1)!='/')
      fullpath+='/';
    fullpath+=prmd;

    str+='<div class="thumbouter" style="width:'+colwid+'px;'; 
    str+='"';
    if(!ipod) {
      str+=' onmouseover="this.className=\'thumbouterhvr\'" onmouseout="this.className=\'thumbouter\'"';
    }
    str+='>';

    //str+='<a onclick="javascript:listdir(\''+doubledelims(fullpath)+'\');" href="#">';
    //str+='<img src="img/folderbig.png" border="0"';
    //str+=' style="padding: 14px 0px 13px 0px;"';
    //str+='></a><br>';

    str+='<div class="novidlink" id="fl_'+i+'">';
    str+='<a onclick="javascript:listdir(\''+doubledelims(fullpath)+'\');" href="#">';
    str+='<img class="vidplaybluebigbtn2" src="img/folderbig.png"';
    str+='></a>';
    str+='</div><br>';

    str+='<a onclick="javascript:listdir(\''+doubledelims(fullpath)+'\');" href="#">';
    str+='&lt; '+truncname(unescape(prmd),26)+' &gt;';
    str+='</a>';
    str+='<br>&nbsp;<br>';
    str+='</div>';

  }

  return str;
}

function flipfiledivbox(_on, _i, _curplayidx) {
  var _divname='fo_'+_i;
  var _divname2='fo2_'+_i;
  var _divname3='fl_'+_i;
  var _cls;
  var _cls2;

  if(_on){
    document.getElementById(_divname).className='thumbouterhvr_vert';
    document.getElementById(_divname2).className='thumboutertxthvr';
    document.getElementById(_divname3).style.borderColor='#f8f8f8';
  } else {
    if(_i == _curplayidx) {
      _cls='thumbouterhvr_vert';
      _cls2='thumboutertxtcur';
    } else {
      _cls='thumbouter_vert';
      _cls2='thumboutertxt';
    }

    document.getElementById(_divname).className=_cls;
    document.getElementById(_divname2).className=_cls2;
    document.getElementById(_divname3).style.borderColor='#cccccc';
  }
}

function drawfilediv2(dir, retParams, i) {
  var str='';
  var prmf=retParams['f_'+i];
  var prmd=retParams['d_'+i];
  var prmtns=retParams['t_'+i];
  var prmtm=retParams['c_'+i];
  var prmn=retParams['n_'+i];
  var _curfile=unescape(curplayfile);
  var _descwid=146;
  var _curplayidx=-1;
  var _cls='thumbouter_vert';
  var _cls2='thumboutertxt';
  
  if(prmf!=null) {

    if(prmn==null) {
      prmn=prmf;
    }

    var fullpath=escape(dir);
    if(prmd!=null&&prmd.charAt(0)!='/'&& fullpath!=null&&fullpath.length>0&&fullpath.charAt(fullpath.length-1)!='/')
      fullpath+='/';
    fullpath+=prmf;

    if(prmtns==null) prmtns=0;
    else if(prmtns >0) prmtns--;

    //if(curplayfile!='' && unescape(fullpath)==_curfile) {
    //  _curplayidx=i;
    //  _cls='thumbouterhvr_vert';
    //  _cls2='thumboutertxthvr';
    //}

    str+='<div class="'+_cls+'" style="width:'+colwid+'px;"';
    if(!ipod)
      str+=' onmouseover="flipfiledivbox(1, \''+i+'\',\''+_curplayidx+'\');" onmouseout="flipfiledivbox(0, \''+i+'\',\''+_curplayidx+'\');"';
    str+=' id="fo_'+i+'">';

    if(prmtns>0) {
      str+='<a style="position:relative;cursor:hand;" name="'+fullpath+'" onclick="javascript:playfile2(\''+urlizestr(fullpath)+'\',\'fl_'+i+'\');">';
      str+='<div class="vidlink" style="background:url(\'tmn/'+fullpath+'_xn.jpg\') no-repeat center;"';
      if(!ipod) 
        str+=' onmouseover="showxtrainfo2(\''+'fl_'+i+'\',\''+doubledelims(fullpath)+'\','+prmtns+');" onmouseout="hidextrainfo2();"';
      str+=' id="fl_'+i+'">';
      //str+='<span class="vidlinkplaybtn"></span>';
      str+='</div>';
      str+='</a>';

    } else {
      str+='<div class="novidlink"';
      str+=' id="fl_'+i+'"><a name="'+fullpath+'" onclick="javascript:playfile2(\''+urlizestr(fullpath)+'\',\'fl_'+i+'\');"';
      str+=' href="#"><img class="vidplaybluebigbtn" src="img/playbluebig.png" alt="Play"';
      str+='></a> ';
      str+='</div>';
    }

    str+='</td><td style="width:'+_descwid+'px;" valign="center" align="left">';

    str+='<div class="'+_cls2+'"';
    if(!ipod)
      str+=' onmouseover="flipfiledivbox(1, \''+i+'\',\''+_curplayidx+'\');" onmouseout="flipfiledivbox(0, \''+i+'\',\''+_curplayidx+'\');"';
    str+=' id="fo2_'+i+'">';

    str+='<a class="flink" href="#" onclick="javascript:playfile2(\''+urlizestr(fullpath)+'\',\'fl_'+i+'\');"';
    if(uipwd!='') str+='?pass='+uipwd;
    str+='\"/>';
    str+=truncname(unescape(prmn),64, 19);
    str+='</a>';
    if(prmd && prmd>0) 
      str+='<br>'+formatduration(prmd*1000);
     else 
      str+='<br>&nbsp;';

    str+='</div>';
    str+='</div>';

  } else if(prmd) {

    var fullpath=dir;

    if(prmd!=null&&prmd.charAt(0)!='/'&& fullpath!=null&&fullpath.length>0&&fullpath.charAt(fullpath.length-1)!='/')
      fullpath+='/';
    fullpath+=prmd;

    str+='<div class="'+_cls+'" ';
    if(!ipod)
      str+=' onmouseover="flipfiledivbox(1, \''+i+'\');" onmouseout="flipfiledivbox(0, \''+i+'\');"';
    str+=' id="fo_'+i+'">'; 

    str+='<div class="novidlink" id="fl_'+i+'">';
    str+='<a onclick="javascript:listdir(\''+doubledelims(fullpath)+'\');" href="#">';
    str+='<img class="vidplaybluebigbtn" src="img/folderbig.png"';
    str+='></a>';
    str+='</div>';

    str+='</td><td style="width:'+_descwid+'px;" valign="center" align="left">';

    str+='<div class="'+_cls2+'"';
    if(!ipod)
      str+=' onmouseover="flipfiledivbox(1, \''+i+'\');" onmouseout="flipfiledivbox(0, \''+i+'\');"';
    str+=' id="fo2_'+i+'">';

    str+='<a onclick="javascript:listdir(\''+doubledelims(fullpath)+'\');" href="#">';
    str+='&lt; '+truncname(unescape(prmd),60,20)+' &gt;';
    str+='</a>';
    str+='<br>&nbsp;<br>';

    str+='</div>';
    str+='</div>';

  }

  //str +='</td></tr></table>';

  return str;
}

function drawprevnext(startidx,cnt,tot) {
  var str='';
  var stylestr;
  var _s0='';
  var _s0cur='';


  if(ipod>0) {
    stylestr=' style="font-size:19px;"';
  } else {
    _s0=' onmouseover="javascript:this.className=\'prevnextsel\'" onmouseout="javascript:this.className=\'prevnext\'"';
    _s0cur=' onmouseover="javascript:this.className=\'prevnextsel\'" onmouseout="javascript:this.className=\'prevnextcur\'"';
  }

  var _idx2=0;
  var _min=Number(startidx)-(5 * curmaxpp);
  if(_min<0) _min=0;

  str+='<table border="0" cellpadding="1" cellspacing="4"><tr>'; 

  if(_min>0) {
    str+='<td align="center" '+stylestr+'><div class="prevnext" '+_s0+'>';
    str+='&nbsp;<a onclick="javascript:'+frameprfx()+'listdir(\''+curdir+'\',\''+getsearchstr()+'\','+0+');" href="#">First</a>&nbsp;';
    str+='</div></td>';
  }

  if(startidx>0) {
    var _sidx=Number(startidx)-Number(curmaxpp);
    str+='<td align="center" '+stylestr+'><div class="prevnext" '+_s0+'>'; 
    str+='<a onclick="javascript:'+frameprfx()+'listdir(\''+curdir+'\',\''+getsearchstr()+'\','+_sidx+');" href="#">&lt;&lt;</a>';
    str+='</div></td>';
  } else {
    str+='<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>';
  }
  if(Number(cnt)<Number(tot)) {
    for(var idx=_min;idx<tot&&_idx2<11;idx+=curmaxpp) {
      _idx2++;
      _dispidx=+(Number(_min)/Number(curmaxpp))+Number(_idx2);
      str+='<td align="center" '+stylestr+'>';

      if(startidx==idx) str+='<div class="prevnextcur" '+_s0cur+'>&nbsp;<b>'+_dispidx+'</b>&nbsp;';
      else str+='<div class="prevnext" '+_s0+'>&nbsp;<a onclick="javascript:'+frameprfx()+'listdir(\''+curdir+'\',\''+getsearchstr()+'\','+idx+');" href="#">'+_dispidx+'</a>&nbsp;';
      str+='</div></td>';
    }
  }

  if((Number(startidx)+Number(curmaxpp))<tot) {
    var _sidx=Number(startidx)+Number(curmaxpp);
    str+='<td align="center" '+stylestr+'><div class="prevnext" '+_s0+'>';
    str+='<a onclick="javascript:'+frameprfx()+'listdir(\''+curdir+'\',\''+getsearchstr()+'\','+_sidx+');" href="#">&gt;&gt;</a>';
    str+='</div></td>';
  }

  str+='</tr></table>';

  //parent.document.getElementById('tester').innerHTML='startidx: '+startidx+' cnt:'+cnt+' tot:'+tot+' curmaxpp:'+curmaxpp+' _min:'+_min; 

  return str;
}


function drawfilelist(txt) {
  var colidx=0;
  var str='';
  var retParams=getRespArray(txt); 
  var cnt=retParams['cnt'];
  var tot=retParams['tot'];
  var dir=retParams['dir'];
  var startidx=retParams['idx'];
  var medialistobj=document.getElementById('medialist');

  if(medialistobj==null) 
    return;

  if(curdir!=dir) {
    var _e=getsearchstrobj();
    if(_e!=null)
      _e.value='';
  }
  curdir=dir;

  if(shownavigate) {
    var strdir='Navigate: ';
    if(dir=='') 
      strdir+='<a onclick="javascript:'+frameprfx()+'listdir(\'\');" href="#">&lt;Home&gt;</a>';
    else if(dir!=null) 
      strdir+=dispdirstr(dir, 'top.frames[\'listframe\'].');
    var obj=parent.document.getElementById('parentcurdirstr');
    if(obj!=null)
      obj.innerHTML=strdir;
  }

  if(cnt<0) {
    str+='<br>&nbsp;<br>&nbsp;<br><font class="cstatusred">Please set the media directory in vsx.conf</font>';
  } else if(cnt==0) {
    parentdir=getparentdir(dir,'/');
    str+='<div nowrap>';
    str+='&nbsp;&nbsp;';
    if(getsearchstr()!='')
      str+='No results found.';
    else 
      str+='No media files here.&nbsp';

    if(shownavigateback)
      str+='&nbsp;<a onclick="javascript:'+frameprfx()+'listdir(\''+doubledelims(parentdir)+'\');" href="#">Go Back</a>';
    str+='</div>';
  } else {

    if(colvert==1&&cnt>0) {

      if(ipod>0) str+='&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;';
      str+='&nbsp;&nbsp;Showing '+(Number(startidx)+1);
      if(cnt>1)
        str+=' - '+(Number(startidx)+Number(cnt));
      if(tot!=cnt) {
        str+=' out of '+tot;
      }
      str+=' video'+(cnt>1?'s':'');
   
      parentdir=getparentdir(dir,'/');
      if(dir!=null&&dir!='') {
        if(shownavigateback)
          str+='&nbsp;&nbsp;<a onclick="javascript:'+frameprfx()+'listdir(\''+doubledelims(parentdir)+'\');" href="#">Go Back</a>';
        str+='<br>&nbsp;';
      }
      str+='<br><div width="100%" align="center"><font style="font-size:13px;"><br>';
      str+=drawprevnext(startidx,cnt,tot);
      str+='</font></div><br>';
    }


    if(colvert==1) {
      if(ipod>0) {
        str+='<table border="0" cellpadding="2" cellspacing="2"><tr><td>';
        str+='<img src="img/h_trans.gif"></td><td>';
      }
      str+='<table border="0" cellpadding="2" cellspacing="0"><tr>';
      for(var i=0; i<cnt; i++) {
        if(colcnt==1) {
          str+='<td style="width:'+colwid+'px; height:5px;" valign="top" align="center"></td></tr><tr>';
          str+='<td style="width:'+colwid+'px;" valign="top" align="center">';
          str+=drawfilediv2(dir, retParams, i);
          str+='</td>';
        } else {
          str+='<td style="width:'+colwid+'px;" valign="top" align="center">';
          str+=drawfilediv(dir, retParams, i);
          str+='</td>';
        }
        if(++colidx>=colcnt) {
          str+='</tr><tr>';
          colidx=0;
        }
      }
      str+='</tr></table>';
      if(ipod>0) {
        str+='</td></tr></table>';
      }

      str+='<br><div width="100%" align="center"><font style="font-size:13px;">';
      str+=drawprevnext(startidx,cnt,tot);
      str+='</font></div>';

    } else {

      var _wid=truebody(document).clientWidth-20;
      var _num=Math.floor(_wid/(Number(colwid)+10));
      str+='<table border="1" cellpadding="2" cellspacing="2"><tr>';
      for(var i=0; i<cnt; i++) {
        if(++colidx>_num){
          str+='</tr><tr>';
          colidx=0; 
        }
        str+='<td style="width:'+colwid+'px;" valign="top" align="center">';
        str+=drawfilediv(dir, retParams, i);
        str+='</td>';
      }
      str+='</tr></table>';

    }
  }

  medialistobj.innerHTML=str;
}


function video_hide() {
  divobj=document.getElementById('inlineplayerdiv');
  if(divobj!=null)
  divobj.innerHTML='';
}

function video_tag_str(url) {
  var _w=320;
  if(ipod==2)_w=540;
  var _h=Number(_w)*Number(.6);
  var stylestr='';

  var str='<div style="margin:0px 0px 0px 0px; padding: 16px 0px 0px 0px">';
  str+='<video class="vidtag" id="vplayobj" controls="true" autoplay="true" src="'+url+'" style="width:'+_w+'px; height:'+_h+'px;"/></div>';

  if(ipod>0) {
    stylestr=' style="font-size:19px;"';
  }
  str+='<a href="#" '+stylestr+' onclick="javascript:video_hide();">Hide</a>';

  return str;
}

function cbOnSuccess(id, txt) {

  if(id == 1) {
    var retParams=getRespArray(txt); 
    var _a=retParams['action'];
    var _m=retParams['method'];
    var _m2=retParams['methodAuto'];

    //alert('m:'+_m+' m2:'+_m2+' a:'+_a);

    if((_m==4||_m2==4)&&(_a==2||_a==5)) {

      //todo: rtsp link inside iframe
      divobj=parent.document.getElementById('inlineplayerdiv');
      if(divobj==null)
        divobj=document.getElementById('inlineplayerdiv');
      var strUrl='/live/'+curfullpath;
      var str='<iframe id="mplayerframe" name="mplayerframe" src="';
      str+=strUrl+'" frameborder="0" width="100%" height="100%">';
      str+='<p>Your browser does not support iframes.</p></iframe>';
      if(divobj!=null)
        divobj.innerHTML=str;

    } else 
    if((_m==2||_m2==2)&&(_a==2||_a==5)) {

      var str='';
      var url='/httplive/'+curfullpath+'/out.m3u8';
      if(prevplaydiv!=null) {
        prevplaydiv.innerHTML='';
      }
      divobj=parent.document.getElementById('inlineplayerdiv');
      if(divobj==null)
        divobj=document.getElementById('inlineplayerdiv');
      if(divobj!=null) {
        //vidtagobj=divobj;
        //vidtagurl=url;
        //ajaxGet(listReq, url+'?check=12', async_update_vidtag, null, 1);
        str=video_tag_str(url);
        divobj.innerHTML=str;
      }
      prevplaydiv=divobj;
    } else {
      playfile(curfullpath, curplaydivname);
    }


  } else if(id == 2) {
    drawfilelist(txt);
  }
}

function async_update_vidtag(id, txt) {

  var str=video_tag_str(vidtagurl);
  vidtagobj.innerHTML=str;

}

function listdir(dir, searchstr, startidx) {

  if(dir.length>0&&dir[dir.length-1]=='#')
    dir=dir.substring(0,dir.length-1);

  var strUrl='?dir='+dir;
  _elem=document.getElementById('dirsort');
  if(_elem==null)
    _elem=parent.document.getElementById('dirsort');
  if(_elem!=null)
    strUrl+='&dirsort='+_elem.options[_elem.selectedIndex].value;
  if(startidx!=null&&startidx>0)
    strUrl+='&start='+startidx; 
  strUrl+='&max='+curmaxpp;
  if(searchstr!=null&&searchstr.length>0&&searchstr!=' ') strUrl+='&search='+searchstr;
  if(uipwd!='') strUrl+='&pass='+uipwd;
  curstartidx=startidx;
  //alert('doing /list'+strUrl +'__dir:__' + dir + '__');
  ajaxGet(listReq, '/list'+strUrl, cbOnSuccess, null, 2);
}

function playfile2(fullpath,divname) {
  if(ipod==1||ipod==2||ipod==3) {
    curfullpath=fullpath;
    curplaydivname=divname;
    var strUri='?check=1';
    ajaxGet(mediaReq, '/live/'+fullpath+strUri, cbOnSuccess, null, 1);
    return;
  } else {
    return playfile(fullpath,divname);
  }

}

function getsearchstrobj() {
  var _e=parent.document.getElementById('searchinp');
  if(_e==null)
    _e=document.getElementById('searchinp');
  return _e;
}

function getsearchstr() {
  var _s;
  var _e=getsearchstrobj();
  if(_e)
    _s=_e.value;
  return _s;
}

function playfile(fullpath,divname) {
  var doc=parent.document;
  if(doc==null) doc = document;
  _mplayerdiv=parent.document.getElementById('mplayerdiv');

  if(_mplayerdiv!=null) {

    var strUrl='/live/'+fullpath+'?nologo=1';
    //strUrl+='&file='+fullpath;
    var str='<iframe id="mplayerframe" name="mplayerframe" src="';
    str+=strUrl+'" frameborder="0" width="100%" height="100%">';
    str+='<p>Your browser does not support iframes.</p></iframe>';

    parent.document.getElementById('mplayerdiv').innerHTML=str;
  } else {

    _elem=document.getElementById('inlineplayerdiv');
    if(_elem==null)
      _elem=parent.document.getElementById('inlineplayerdiv');
    var strUrl='/live/'+fullpath;
    var str=video_tag_str('/live/'+fullpath);
    if(ipod==2) {
      str+='<div id="fileinfo"></div><div id="fileinfo3"></div>';
    }
    _elem.innerHTML=str;

    _wid=80;
    scroll(_wid,0);
    if((_elempl=document.getElementById('vplayobj'))!=null) {
      _elempl.load();
      _elempl.play();
    }
    //window.location='/live/'+fullpath;

    if(ipod==2) {
      var profile='';
      ajaxGet(fileInfoReqFl, '/list?chkprofile=1&profile='+profile+'&file='+fullpath, cbMPInfo, null, 3);
    }


  }

  curplayfile=fullpath;

  listdir(''+curdir+'', getsearchstr(),curstartidx);

  //ajaxGet(mediaReq, '/live/'+fullpath, cbOnSuccess, null, 1);
}

function dirsortchange(_elem) {
  var str=_elem.options[_elem.selectedIndex].value;
  listdir(curdir,getsearchstr());
}

function drawsortbox(divname) {
  var str='&nbsp;Sort: &nbsp; ';
  str+='<select id="dirsort" onchange="javascript:'+frameprfx()+'dirsortchange(this);">';
  str+='<option value="6">Newest</option>';
  str+='<option value="5">Oldest</option>';
  str+='<option value="1">Name</option>';
  str+='<option value="2">Reverse </option>';
  str+='<option value="4">Largest</option>';
  str+='<option value="3">Smallest</option>';
  str+='<option value="0">None</option>';
  str+='</select>';
  if(divname!=null)
    divname.innerHTML=str;
}

function dosearch(_elem) {
  //parent.document.getElementById('tester').innerHTML=_elem.value;
  
  listdir(curdir, _elem.value);

}

function drawsearchbox(divname) {
  var str='<div style=\"padding:0px 16px 0px 0px\">Search: &nbsp; ';
  str+='<input id="searchinp" type="text" size="24" onkeyup="javascript:'+frameprfx()+'dosearch(this);" ';
  str+=' class="inp" onfocus="javascript:this.className=\'inpsel\';" onblur="javascript:this.className=\'inp\';"';
  str+='/></div>';
  if(divname!=null)
    divname.innerHTML=str;
}


function showxtrainfo2(divname,fpath,numtn){
 
  //parent.document.getElementById('tester').innerHTML='showxtrainfo2 '+numtn + ' '+new Date().getTime()+' imgrolldiv:'+imgrolldiv+' divname:'+divname;

  if(divname!=imgrolldiv) {
    imgrollidx=0;
    imgrolldiv=divname;
    imgrollsrc='tmn/'+fpath;
    if(numtn>0) {
      curhvrnumtn=numtn;
      rollimage2();
    }
  }
}

function hidextrainfo2(){
  clearTimeout(timerimgroll);
  var _elem=document.getElementById(imgrolldiv);
  if(_elem!=null) {
    _elem.style.backgroundImage='url(\''+imgrollsrc+'_xn.jpg\')';
  }
  imgrolldiv='';
  //parent.document.getElementById('tester2').innerHTML='hidextrainfo2 '+new Date().getTime();
}

function rollimage2() {
  var _elem=document.getElementById(imgrolldiv);
  var curtm=new Date().getTime();


  if(_elem!=null) {
    var tmtms=700;
    _elem.style.backgroundImage='url(\''+imgrollsrc+'_xn'+imgrollidx+'.jpg\')';

  //parent.document.getElementById('tester3').innerHTML='rollimage2 '+imgrollidx+' curtm:'+curtm+' imgrolltm:'+imgrolltm+' imgrollidx'+imgrollidx;

    if(imgrolltm==0 || imgrolltm+tmtms<curtm){
      if(++imgrollidx>curhvrnumtn-1) {
        imgrollidx=0;
        tmtms=3000;
      } else if(imgrollidx==curhvrnumtn-1){
        tmtms=700;
      }
      imgrolltm=curtm;
    }

    timerimgroll=setTimeout("rollimage2();", tmtms);
  } else {
    hidextrainfo2();
  }


}

