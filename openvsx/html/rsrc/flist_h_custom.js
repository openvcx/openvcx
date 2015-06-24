
function init_flist_custom() {

  h_objs = new Array();
  var idx=0;

  h_objs[idx++] = init_v('L01.sdp', 2, 'Test Stream 1 - Live');
  h_objs[idx++] = init_v('L01.sdp', 2, 'Test Stream 2 - Live');
  h_objs[idx++] = init_v('L01.sdp', 2, 'Test Stream 3 - Live');
  h_objs[idx++] = init_v('L01.sdp', 2, 'Test Stream 4 - Live');
  h_objs[idx++] = init_v('L01.sdp', 2, 'Test Stream 5 - Live');
  h_objs[idx++] = init_v('L01.sdp', 2, 'Test Stream 6 - Live');


  var obj=parent.document.getElementById('listframediv_b_title');
  if(obj!=null) {
    var str='';
    if(ipod>0) str+='<br>';
    else str+='&nbsp;&nbsp;&nbsp;&nbsp;';
    str+='&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<b>Live Streams</b>';
    obj.innerHTML=str;
  }

}
