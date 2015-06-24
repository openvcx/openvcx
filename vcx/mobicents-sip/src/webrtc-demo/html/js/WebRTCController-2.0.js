
function WebRTCController(view) {
    console.debug("WebRTCController:WebRTCController()")
    //  WebRtcComm client 
    this.view=view;
    this.webRtcCommClient=new WebRtcCommClient(this); 
    this.webRtcCommClientConfiguration=undefined;
    this.localAudioVideoMediaStream=undefined;
    this.webRtcCommCall=undefined;
    this.sipContactUri=WebRTCController.prototype.DEFAULT_SIP_CONTACT;
    this.sipUserName=WebRTCController.prototype.DEFAULT_SIP_USER_NAME;
}

WebRTCController.prototype.constructor=WebRTCController;


WebRTCController.prototype.DEFAULT_SIP_OUTBOUND_PROXY=g_vcxhost;
//WebRTCController.prototype.DEFAULT_SIP_USER_AGENT="WebRTCPhone/0.0.1";
WebRTCController.prototype.DEFAULT_SIP_USER_AGENT_CAPABILITIES="";
WebRTCController.prototype.DEFAULT_SIP_DOMAIN=g_domain;
WebRTCController.prototype.DEFAULT_SIP_DISPLAY_NAME=g_uid;
WebRTCController.prototype.DEFAULT_SIP_USER_NAME=g_uid;
WebRTCController.prototype.DEFAULT_SIP_LOGIN=g_uid;
WebRTCController.prototype.DEFAULT_SIP_PASSWORD="default";
WebRTCController.prototype.DEFAULT_SIP_CONTACT=g_to;
WebRTCController.prototype.DEFAULT_SIP_REGISTER_MODE=true;
WebRTCController.prototype.DEFAULT_STUN_SERVER=g_stunserver;



/**
 * on load event handler
 */ 
WebRTCController.prototype.onLoadViewEventHandler=function() 
{
    console.debug ("WebRTCController:onLoadViewEventHandler()");

    var clientUA=navigator.userAgent;
    var userAgent="WebRTCPhone";

    if(navigator.webkitGetUserMedia) {
        // Google Chrome user agent
        if(clientUA.match('Android')!=null) {
          userAgent+="/webkit/android";
        } else {
          userAgent+="/webkit/unknown";
        }
    } else if(navigator.mozGetUserMedia) {
        // Mozilla firefox  user agent
        userAgent+="/mozilla/unknown";
    }
    userAgent+="/1.0.0";
        
    // Setup SIP default Profile
    this.webRtcCommClientConfiguration =  { 
        communicationMode:WebRtcCommClient.prototype.SIP,
        sip:{
            //sipUserAgent:this.DEFAULT_SIP_USER_AGENT,
            sipUserAgent:userAgent,
            sipOutboundProxy:this.DEFAULT_SIP_OUTBOUND_PROXY,
            sipDomain:this.DEFAULT_SIP_DOMAIN,
            sipDisplayName:this.DEFAULT_SIP_DISPLAY_NAME,
            sipUserName:this.DEFAULT_SIP_USER_NAME,
            sipLogin:this.DEFAULT_SIP_LOGIN,
            sipPassword:this.DEFAULT_SIP_PASSWORD,
            sipUserAgentCapabilities:this.DEFAULT_SIP_USER_AGENT_CAPABILITIES,
            sipRegisterMode:this.DEFAULT_SIP_REGISTER_MODE
        },
        RTCPeerConnection:
        {
            stunServer:this.DEFAULT_STUN_SERVER         
        }
    } 
    
    // Setup SIP overloaded profile configuration in request URL       
    if(this.view.location.search.length>0)
    {
        var argumentsString = this.view.location.search.substring(1);
        var arguments = argumentsString.split('&');
        if(arguments.length==0) arguments = [argumentsString];
        for(var i=0;i<arguments.length;i++)
        {   
            var argument = arguments[i].split("=");
            if("sipUserName"==argument[0])
            {
                this.webRtcCommClientConfiguration.sip.sipUserName =argument[1];
                if(this.webRtcCommClientConfiguration.sip.sipUserName=="") this.webRtcCommClientConfiguration.sip.sipUserName=undefined;
            } 
            else if("sipDomain"==argument[0])
            {
                this.webRtcCommClientConfiguration.sip.sipDomain =argument[1];
                if(this.webRtcCommClientConfiguration.sip.sipDomain=="") this.webRtcCommClientConfiguration.sip.sipDomain=undefined;
            } 
            else if("sipDisplayedName"==argument[0])
            {
                this.webRtcCommClientConfiguration.sip.sipDisplayName =argument[1];
                if(this.webRtcCommClientConfiguration.sip.sipDisplayName=="") this.webRtcCommClientConfiguration.sip.sipDisplayedName=undefined;
            } 
            else if("sipPassword"==argument[0])
            {
                this.webRtcCommClientConfiguration.sip.sipPassword =argument[1];
                if(this.webRtcCommClientConfiguration.sip.sipPassword=="") this.webRtcCommClientConfiguration.sip.sipPassword=undefined;
            } 
            else if("sipLogin"==argument[0])
            {
                this.webRtcCommClientConfiguration.sip.sipLogin =argument[1];
                if(this.webRtcCommClientConfiguration.sip.sipLogin=="") this.webRtcCommClientConfiguration.sip.sipLogin=undefined;
            }
            else if("sipContactUri"==argument[0])
            {
                this.sipContactUri =argument[1];
            }
        }
    }  


    this.initViewSettings();   
}

/**
 * on load event handler
 */
WebRTCController.prototype.onStartViewEventHandler=function()
{
    console.debug ("WebRTCController:onStartViewEventHandler()");

    this.initView();   

}

/**
 * on unload event handler
 */ 
WebRTCController.prototype.onUnloadViewEventHandler=function()
{
    console.debug ("WebRTCController:onBeforeUnloadEventHandler()"); 
    if(this.webRtcCommClient != undefined)
    {
        try
        {
            this.webRtcCommClient.close();  
        }
        catch(exception)
        {
             console.error("WebRtcCommTestWebAppController:onUnloadViewEventHandler(): caught exception:"+exception);  
        }
    }    
}


WebRTCController.prototype.initViewSettings=function(){
    console.debug ("WebRTCController:initViewSettings()");  
    this.view.disableCallButton();
    this.view.disableEndCallButton();
    this.view.enableVideoCheckbox();
    this.view.disableDisconnectButton();
    this.view.disableConnectButton();
    this.view.stopLocalVideo();
    this.view.hideLocalVideo();
    this.view.stopRemoteVideo();
    this.view.hideRemoteVideo();
    this.view.setStunServerTextInputValue(this.webRtcCommClientConfiguration.RTCPeerConnection.stunServer);
    this.view.setSipOutboundProxyTextInputValue(this.webRtcCommClientConfiguration.sip.sipOutboundProxy);
    this.view.setSipDomainTextInputValue(this.webRtcCommClientConfiguration.sip.sipDomain);
    this.view.setSipDisplayNameTextInputValue(this.webRtcCommClientConfiguration.sip.sipDisplayName);
    if(this.sipUserName!='' && this.sipUserName!=undefined)
      this.view.setSipUserNameTextInputValue(this.sipUserName);
    //this.view.setSipUserNameTextInputValue(this.webRtcCommClientConfiguration.sip.sipUserName);
    this.view.setSipLoginTextInputValue(this.webRtcCommClientConfiguration.sip.sipLogin);
    this.view.setSipPasswordTextInputValue(this.webRtcCommClientConfiguration.sip.sipPassword);
    if(this.sipContactUri!='' && this.sipContactUri!=undefined)
      this.view.setSipContactUriTextInputValue(this.sipContactUri);

}
    
WebRTCController.prototype.initView=function(){
    console.debug ("WebRTCController:initView() video: " + g_enableVideo);  

    // Get local user media
    try
    {
        var that = this;
        if(navigator.webkitGetUserMedia)
        {
//if(this.localAudioVideoMediaStream!=undefined) {
//    this.localAudioVideoMediaStream.stop();
//    this.localAudioVideoMediaStream=undefined;
//}
            // Google Chrome user agent
            navigator.webkitGetUserMedia({
                audio:true, 
                video: g_enableVideo ? { mandatory: { maxWidth: g_maxWidth, maxHeight:g_maxHeight} } : false
                //video:  { mandatory: { maxWidth: g_maxWidth, maxHeight:g_maxHeight} } 
                //video:g_enableVideo 
            }, function(localMediaStream) {
                that.onGetUserMediaSuccessEventHandler(localMediaStream);
            }, function(error) {
                that.onGetUserMediaErrorEventHandler(error);
            });

        }
        else if(navigator.mozGetUserMedia)
        {
            // Mozilla firefox  user agent
            navigator.mozGetUserMedia({
                audio:true,
                //video:true
                video: g_enableVideo ? { mandatory: { maxWidth: g_maxWidth, maxHeight:g_maxHeight} } : false
            },function(localMediaStream) {
                that.onGetUserMediaSuccessEventHandler(localMediaStream);
            },function(error) {
                that.onGetUserMediaErrorEventHandler(error);
            });
        }
        else
        {
            console.error("WebRTCController:onLoadEventHandler(): navigator doesn't implement getUserMedia API.  ")
            modal_alert("Your browser is not compatible with WebRTC.  Please try Chrome or Firefox.")     
        }

    }
    catch(exception)
    {
        console.error("WebRTCController:onLoadEventHandler(): caught exception: "+exception);
        modal_alert("WebRTCController:onLoadEventHandler(): caught exception: "+exception);
    }   
}
   
/**
 * get user media success event handler (Google Chrome User agent)
 * @param localAudioVideoMediaStream object
 */ 
WebRTCController.prototype.onGetUserMediaSuccessEventHandler=function(localAudioVideoMediaStream) 
{
    try
    {
        console.debug("WebRTCController:onGetUserMediaSuccessEventHandler(): localAudioVideoMediaStream.id="+localAudioVideoMediaStream.id);


        this.localAudioVideoMediaStream=localAudioVideoMediaStream;
        this.view.playLocalVideo(this.localAudioVideoMediaStream);
        this.view.showLocalVideo();
        this.view.enableConnectButton();          
    }
    catch(exception)
    {
        console.debug("WebRTCController:onGetUserMediaSuccessEventHandler(): caught exception: "+exception);
    }
}           
 
WebRTCController.prototype.onGetUserMediaErrorEventHandler=function(error) 
{
    console.debug("WebRTCController:onGetUserMediaErrorEventHandler(): error="+error);
    modal_alert("Failed to get local user media: error="+error);
}	
  

var is_registered=false;
var is_connected=false;
var timeout_reg;
var doRegister_ctxt;
function doRegister(_this){
  if(null==_this)
      _this = doRegister_ctxt;
  clearTimeout(timeout_reg);

  try {
      _this.webRtcCommClient.open(_this.webRtcCommClientConfiguration); 
      _this.view.disableConnectButton();
  } catch(exception) {
      modal_alert("Unable to register. "+exception)  
  }

}
/**
 * on connect event handler
 */ 
WebRTCController.prototype.onClickConnectButtonViewEventHandler=function()
{
    console.debug ("WebRTCController:onClickConnectButtonViewEventHandler()"); 

    if(this.webRtcCommClient != undefined)
    {
        try
        {


            this.webRtcCommClientConfiguration.RTCPeerConnection.stunServer= this.view.getStunServerTextInputValue();
            this.webRtcCommClientConfiguration.sip.sipOutboundProxy = this.view.getSipOutboundProxyTextInputValue();
            this.webRtcCommClientConfiguration.sip.sipDomain = this.view.getSipDomainTextInputValue();
            this.webRtcCommClientConfiguration.sip.sipDisplayName= this.view.getSipDisplayNameTextInputValue();
            this.webRtcCommClientConfiguration.sip.sipUserName = this.view.getSipUserNameTextInputValue();
            //this.webRtcCommClientConfiguration.sip.sipLogin = this.view.getSipLoginTextInputValue();
            this.webRtcCommClientConfiguration.sip.sipLogin = this.webRtcCommClientConfiguration.sip.sipUserName;
            this.webRtcCommClientConfiguration.sip.sipPassword = this.view.getSipPasswordTextInputValue();
            //this.webRtcCommClient.open(this.webRtcCommClientConfiguration); 
            //this.view.disableConnectButton();

            if(false&&this.webRtcCommClientConfiguration.sip.sipOutboundProxy.indexOf('ngvid.com') >= 0) {
                modal_alert("The server at ngvid.com is currently available for a private demo only.");
                doRegister_ctxt=this;
                timeout_reg = setTimeout(doRegister, 2000);
            } else {
                this.webRtcCommClient.open(this.webRtcCommClientConfiguration); 
                this.view.disableConnectButton();
            }

        }
        catch(exception)
        {
            modal_alert("Unable to register. "+exception)  
        }
    }
    else
    {
        console.error("WebRTCController:onClickConnectButtonViewEventHandler(): internal error");      
    }
}


/**
 * on disconnect event handler
 */ 
WebRTCController.prototype.onClickDisconnectButtonViewEventHandler=function()
{
    console.debug ("WebRTCController:onClickDisconnectButtonViewEventHandler()"); 
    if(this.webRtcCommClient != undefined)
    {
        try
        {
            this.webRtcCommClient.close();  
        }
        catch(exception)
        {
            modal_alert("Disconnection has failed, reason:"+exception)  
        }
    }
    else
    {
        console.error("WebRTCController:onClickDisconnectButtonViewEventHandler(): internal error");      
    }
}

/**
 * on call event handler
 */ 
WebRTCController.prototype.onClickCallButtonViewEventHandler=function(calleePhoneNumber)
{
    console.debug ("WebRTCController:onClickCallButtonViewEventHandler()"); 
    if(this.webRtcCommClient != undefined)
    {
        try
        {
            var callConfiguration = {
                displayName:this.DEFAULT_SIP_DISPLAY_NAME,
                localMediaStream: this.localAudioVideoMediaStream,
                audioMediaFlag:true,
                videoMediaFlag:g_enableVideo,
                messageMediaFlag:false
            }
            this.webRtcCommCall = this.webRtcCommClient.call(calleePhoneNumber, callConfiguration);
            this.view.disableCallButton();
            this.view.disableDisconnectButton();
            this.view.disableVideoCheckbox();
        }
        catch(exception)
        {
            modal_alert("Call has failed, reason:"+exception)  
        }
    }
    else
    {
        console.error("WebRTCController:onClickCallButtonViewEventHandler(): internal error");      
    }
}


/**
 * on call event handler
 */ 
WebRTCController.prototype.onClickEndCallButtonViewEventHandler=function()
{
    console.debug ("WebRTCController:onClickEndCallButtonViewEventHandler()"); 
    if(this.webRtcCommClient != undefined)
    {
        try
        {
            this.webRtcCommCall.close();
        }
        catch(exception)
        {
            modal_alert("End has failed, reason:"+exception)  
        }
    }
    else
    {
        console.error("WebRTCController:onClickEndCallButtonViewEventHandler(): internal error");      
    }
}

/**
 * on accept event handler
 */ 
WebRTCController.prototype.onClickAcceptCallButtonViewEventHandler=function()
{
    console.debug ("WebRTCController:onClickAcceptCallButtonViewEventHandler()"); 
    if(this.webRtcCommClient != undefined)
    {
        try
        {
            var callConfiguration = {
                displayName:this.DEFAULT_SIP_DISPLAY_NAME,
                localMediaStream: this.localAudioVideoMediaStream,
                audioMediaFlag:true,
                videoMediaFlag:g_enableVideo,
                messageMediaFlag:false
            }
            this.webRtcCommCall.accept(callConfiguration);
            this.view.enableEndCallButton();
            this.view.enableVideoCheckbox();
            this.view.stopRinging();
        }
        catch(exception)
        {
            modal_alert("End has failed, reason:"+exception)  
        }
    }
    else
    {
        console.error("WebRTCController:onClickAcceptCallButtonViewEventHandler(): internal error");      
    }
}

/**
 * on accept event handler
 */ 
WebRTCController.prototype.onClickRejectCallButtonViewEventHandler=function()
{
    console.debug ("WebRTCController:onClickRejectCallButtonViewEventHandler()"); 
    if(this.webRtcCommClient != undefined)
    {
        try
        {
            this.webRtcCommCall.reject();
            this.view.enableCallButton();
            this.view.enableVideoCheckbox();
            this.view.enableDisconnectButton();
            this.view.stopRinging();
        }
        catch(exception)
        {
            modal_alert("End has failed, reason:"+exception)  
        }
    }
    else
    {
        console.error("WebRTCController:onClickRejectCallButtonViewEventHandler(): internal error");      
    }
}



/**
  * Implementation of the WebRtcCommClient listener interface
  */
WebRTCController.prototype.onWebRtcCommClientOpenedEvent=function()
{
    console.debug ("WebRTCController:onWebRtcCommClientOpenedEvent()");
    //Enabled button DISCONNECT, CALL diable CONNECT and BYE
    this.view.enableDisconnectButton();
    this.view.enableCallButton();
    this.view.enableVideoCheckbox();
    this.view.disableConnectButton();
    this.view.disableEndCallButton();
    this.view.ng_createConfNumber();
    is_registered=true;
    modal_alert("Succesfully registered"); 
}
    
WebRTCController.prototype.onWebRtcCommClientOpenErrorEvent=function(error)
{
    console.debug ("WebRTCController:onWebRtcCommClientOpenErrorEvent():error:"+error); 
    this.view.enableConnectButton();
    this.view.disableDisconnectButton();
    this.view.disableCallButton();
    this.view.enableVideoCheckbox();
    this.view.disableEndCallButton();
    this.webRtcCommCall=undefined;
    if(!is_registered)
        modal_alert("Failed to register.  <br>Please check the the NGVX Server address."); 
    else
        modal_alert("Connection to the NGVX server has failed."); 
} 
    
/**
 * Implementation of the WebRtcCommClient listener interface
 */
WebRTCController.prototype.onWebRtcCommClientClosedEvent=function()
{
    console.debug ("WebRTCController:onWebRtcCommClientClosedEvent()"); 
    //Enabled button CONNECT, disable DISCONECT, CALL, BYE
    this.view.enableConnectButton();
    this.view.disableDisconnectButton();
    this.view.disableCallButton();
    this.view.enableVideoCheckbox();
    this.view.disableEndCallButton();
    this.webRtcCommCall=undefined;
    if(is_registered)
        modal_alert("You have unregistered"); 
    else
        modal_alert("Failed to register.  <br>Please check the the NGVX Server address."); 
    is_registered=false;
}
    
/**
 * Implementation of the WebRtcCommCall listener interface
 */
WebRTCController.prototype.onWebRtcCommCallClosedEvent=function(webRtcCommCall)
{
    console.debug ("WebRTCController:onWebRtcCommCallClosedEvent(): webRtcCommCall.getId()="+webRtcCommCall.getId()); 

    //Enabled button DISCONECT, CALL
    this.view.enableCallButton();
    this.view.enableVideoCheckbox();
    this.view.enableDisconnectButton();
    this.view.disableEndCallButton();
    this.view.disableConnectButton();
    this.view.stopRemoteVideo();
    this.view.hideRemoteVideo();
    this.webRtcCommCall=undefined;  
    this.view.ng_onLeaveConf();
    if(is_connected)
       modal_alert("You have disconnected from the call."); 
    else
       modal_alert("Failed to join the call."); 
    is_connected=false
    
}
   
   
/**
 * Implementation of the WebRtcCommCall listener interface
 */
WebRTCController.prototype.onWebRtcCommCallOpenedEvent=function(webRtcCommCall)
{
    console.debug ("WebRTCController:onWebRtcCommCallOpenedEvent(): webRtcCommCall.getId()="+webRtcCommCall.getId()); 
   
    this.view.stopRinging();
    this.view.disableCallButton();
    this.view.disableVideoCheckbox();
    this.view.disableDisconnectButton();
    this.view.enableEndCallButton();
    this.view.disableDisconnectButton();
    this.view.disableConnectButton();
    this.view.showRemoteVideo();
    this.view.playRemoteVideo(webRtcCommCall.getRemoteMediaStream());
    this.view.ng_onJoinConf();
    modal_alert("Connected to the call!"); 
    is_connected=true
}

/**
 * Implementation of the WebRtcCommCall listener interface
 */
WebRTCController.prototype.onWebRtcCommCallInProgressEvent=function(webRtcCommCall)
{
    console.debug ("WebRTCController:onWebRtcCommCallInProgressEvent(): webRtcCommCall.getId()="+webRtcCommCall.getId()); 

    modal_alert("Communication in progress"); 
}


/**
 * Implementation of the WebRtcCommCall listener interface
 */
WebRTCController.prototype.onWebRtcCommCallOpenErrorEvent=function(webRtcCommCall, error)
{
    console.debug ("WebRTCController:onWebRtcCommCallOpenErrorEvent(): webRtcCommCall.getId()="+webRtcCommCall.getId()); 

    //Enabled button DISCONECT, CALL
    this.view.enableCallButton();
    this.view.enableVideoCheckbox();
    this.view.enableDisconnectButton();
    this.view.disableEndCallButton();
    this.view.disableConnectButton();
    this.view.hideRemoteVideo();
    this.view.stopRemoteVideo();
    this.view.stopRinging();
    this.webRtcCommCall=undefined;
    modal_alert("Communication failed: error:"+error); 
}

/**
 * Implementation of the WebRtcCommCall listener interface
 */
WebRTCController.prototype.onWebRtcCommCallRingingEvent=function(webRtcCommCall)
{
    console.debug ("WebRTCController:onWebRtcCommCallRingingEvent(): webRtcCommCall.getId()="+webRtcCommCall.getId()); 
    this.webRtcCommCall=webRtcCommCall;
    this.view.playRinging();
    this.view.disableCallButton();
    this.view.disableVideoCheckbox();
    this.view.disableDisconnectButton();
    this.view.disableEndCallButton();
    this.view.disableConnectButton();
    show_desktop_notification("Incoming Call from " + webRtcCommCall.getCallerPhoneNumber());
    $("#call_message").html("<p>Incoming Call from " + webRtcCommCall.getCallerPhoneNumber() +"</p>");
     $('#callModal').modal(); 
}

/**
 * Implementation of the WebRtcCommCall listener interface
 */
WebRTCController.prototype.onWebRtcCommCallRingingBackEvent=function(webRtcCommCall)
{
    console.debug ("WebRTCController:onWebRtcCommCallRingingBackEvent(): webRtcCommCall.getId()="+webRtcCommCall.getId()); 
    this.view.playRinging();
    this.view.disableCallButton();
    this.view.disableVideoCheckbox();
    this.view.disableDisconnectButton();
    this.view.disableEndCallButton();
    this.view.disableConnectButton();
}

    



