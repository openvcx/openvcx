

var colvert=1;
var colvert_h=0;
var curmaxpp=10;
var curmaxpp_h=3;
var colwid_h=190;
var ipod=0;
var shownavigate=false;
var shownavigateback=false;
var showsearchbox=true;
var showsortbox=true;
var showflist=true;
var showflist_h=false;

function initconfig() {
  if(navigator.userAgent.match('iPhone') || navigator.userAgent.match('iPod')) {
    ipod=1;
    colcnt=3;
    curmaxpp=12;
    colwid=186;
  } else if(navigator.userAgent.match('iPad')) {
    ipod=2;
    colcnt=5;
    curmaxpp=15;
    colwid=186;
  } else if(navigator.userAgent.match('Android') || navigator.userAgent.match('BlackBerry')) {
    ipod=3;
    colcnt=3;
    curmaxpp=12;
    colwid=186;
  } else {
    colcnt=1
    if(colcnt==1) colwid=168; else colwid=186; // 16x should match css 
  }
}
