/**
 * Class WebRTCController
 * @public 
 */ 

navigator.getUserMedia = navigator.webkitGetUserMedia || navigator.mozGetUserMedia;
window.URL = window.URL || window.webkitURL;
var is_registered=false;
var is_connected=false;

/**
 * Constructor 
 */ 
function WebRTCController(view) {
    console.debug("WebRTCController:WebRTCController()")
    //  WebRTComm client 
    this.view=view;
    this.webRTCommClient=new WebRTCommClient(this); 
    this.webRTCommClientConfiguration=undefined;
    this.localAudioVideoMediaStream=undefined;
    this.webRTCommCall=undefined;
    this.sipContactUri=WebRTCController.prototype.DEFAULT_SIP_CONTACT;
    this.sipUserName=WebRTCController.prototype.DEFAULT_SIP_USER_NAME;
}

WebRTCController.prototype.constructor=WebRTCController;

// Default SIP profile to use
WebRTCController.prototype.DEFAULT_SIP_OUTBOUND_PROXY=g_vcxhost;
WebRTCController.prototype.DEFAULT_SIP_USER_AGENT="WebRTCPhone/0.0.2" 
WebRTCController.prototype.DEFAULT_SIP_USER_AGENT_CAPABILITIES=""
WebRTCController.prototype.DEFAULT_SIP_DOMAIN=g_domain;
WebRTCController.prototype.DEFAULT_SIP_DISPLAY_NAME=g_uid;
WebRTCController.prototype.DEFAULT_SIP_USER_NAME=g_uid;
WebRTCController.prototype.DEFAULT_SIP_LOGIN=g_uid;
WebRTCController.prototype.DEFAULT_SIP_PASSWORD="default";
WebRTCController.prototype.DEFAULT_SIP_CONTACT=g_to;
WebRTCController.prototype.DEFAULT_SIP_REGISTER_MODE=true;
//WebRTCController.prototype.DEFAULT_STUN_SERVER=g_stunserver;
WebRTCController.prototype.DEFAULT_TURN_SERVER='';
WebRTCController.prototype.DEFAULT_TURN_LOGIN=''; 
WebRTCController.prototype.DEFAULT_TURN_PASSWORD=''; 
WebRTCController.prototype.DEFAULT_AUDIO_CODECS_FILTER=undefined; // RTCPeerConnection default codec filter
WebRTCController.prototype.DEFAULT_VIDEO_CODECS_FILTER=undefined; // RTCPeerConnection default codec filter
WebRTCController.prototype.DEFAULT_LOCAL_VIDEO_FORMAT="{\"mandatory\": {\"maxWidth\": 640}}"
WebRTCController.prototype.DEFAULT_SIP_URI_CONTACT_PARAMETERS=undefined;
WebRTCController.prototype.DEFAULT_DTLS_SRTP_KEY_AGREEMENT_MODE=true;
WebRTCController.prototype.DEFAULT_FORCE_TURN_MEDIA_RELAY_MODE=false;

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
    this.webRTCommClientConfiguration =  { 
        communicationMode:WebRTCommClient.prototype.SIP,
        sip:{
            //sipUserAgent:this.DEFAULT_SIP_USER_AGENT,
            sipUserAgent:userAgent,
            sipOutboundProxy:this.DEFAULT_SIP_OUTBOUND_PROXY,
            sipDomain:this.DEFAULT_SIP_DOMAIN,
            sipDisplayName:this.DEFAULT_SIP_DISPLAY_NAME,
            sipUserName:this.DEFAULT_SIP_USER_NAME,
            sipLogin:this.DEFAULT_SIP_LOGIN,
            sipPassword:this.DEFAULT_SIP_PASSWORD,
            sipUriContactParameters:this.DEFAULT_SIP_URI_CONTACT_PARAMETERS,
            sipUserAgentCapabilities:this.DEFAULT_SIP_USER_AGENT_CAPABILITIES,
            sipRegisterMode:this.DEFAULT_SIP_REGISTER_MODE
        },
        RTCPeerConnection:
        {
            //stunServer:this.DEFAULT_STUN_SERVER,
            stunServer:g_stunserver,
            turnServer:this.DEFAULT_TURN_SERVER, 
            turnLogin:this.DEFAULT_TURN_LOGIN,
            turnPassword:this.DEFAULT_TURN_PASSWORD,
            dtlsSrtpKeyAgreement:this.DEFAULT_DTLS_SRTP_KEY_AGREEMENT_MODE,
            forceTurnMediaRelay:this.DEFAULT_FORCE_TURN_MEDIA_RELAY_MODE
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
                this.webRTCommClientConfiguration.sip.sipUserName =argument[1];
                if(this.webRTCommClientConfiguration.sip.sipUserName=="") this.webRTCommClientConfiguration.sip.sipUserName=undefined;
            } 
            else if("sipDomain"==argument[0])
            {
                this.webRTCommClientConfiguration.sip.sipDomain =argument[1];
                if(this.webRTCommClientConfiguration.sip.sipDomain=="") this.webRTCommClientConfiguration.sip.sipDomain=undefined;
            } 
            else if("sipDisplayName"==argument[0])
            {
                this.webRTCommClientConfiguration.sip.sipDisplayName =argument[1];
                if(this.webRTCommClientConfiguration.sip.sipDisplayName=="") this.webRTCommClientConfiguration.sip.sipDisplayName=undefined;
            } 
            else if("sipPassword"==argument[0])
            {
                this.webRTCommClientConfiguration.sip.sipPassword =argument[1];
                if(this.webRTCommClientConfiguration.sip.sipPassword=="") this.webRTCommClientConfiguration.sip.sipPassword=undefined;
            } 
            else if("sipLogin"==argument[0])
            {
                this.webRTCommClientConfiguration.sip.sipLogin =argument[1];
                if(this.webRTCommClientConfiguration.sip.sipLogin=="") this.webRTCommClientConfiguration.sip.sipLogin=undefined;
            }
            else if("sipContactUri"==argument[0])
            {
                this.sipContactUri =argument[1];
                if(this.webRTCommClientConfiguration.sip.sipContactUri=="") this.webRTCommClientConfiguration.sip.sipContactUri=undefined;
            }
        }
    }  
    this.initView();   
}

/**
 * on unload event handler
 */ 
WebRTCController.prototype.onUnloadViewEventHandler=function()
{
    console.debug ("WebRTCController:onBeforeUnloadEventHandler()"); 
    if(this.webRTCommClient != undefined)
    {
        try
        {
            this.webRTCommClient.close();  
        }
        catch(exception)
        {
             console.error("WebRtcCommTestWebAppController:onUnloadViewEventHandler(): caught exception:"+exception);  
        }
    }    
}


WebRTCController.prototype.initView=function(){
    console.debug ("WebRTCController:initView()");  
    this.view.disableCallButton();
    this.view.disableEndCallButton();
    this.view.enableVideoCheckbox();
    this.view.setDisplayMessage(null);
    //this.view.disableCancelCallButton();
    this.view.disableDisconnectButton();
    this.view.disableConnectButton();
    this.view.stopLocalVideo();
    this.view.hideLocalVideo();
    this.view.stopRemoteVideo();
    this.view.hideRemoteVideo();
    this.view.setStunServerTextInputValue(this.webRTCommClientConfiguration.RTCPeerConnection.stunServer);
    //this.view.setTurnServerTextInputValue(this.webRTCommClientConfiguration.RTCPeerConnection.turnServer);
    //this.view.setTurnLoginTextInputValue(this.webRTCommClientConfiguration.RTCPeerConnection.turnLogin);
    //this.view.setTurnPasswordTextInputValue(this.webRTCommClientConfiguration.RTCPeerConnection.turnPassword);
    this.view.setSipOutboundProxyTextInputValue(this.webRTCommClientConfiguration.sip.sipOutboundProxy);
//    this.view.setSipUserAgentTextInputValue(this.webRTCommClientConfiguration.sip.sipUserAgent);
//    this.view.setSipUriContactParametersTextInputValue(this.webRTCommClientConfiguration.sip.sipUriContactParameters);
//    this.view.setSipUserAgentCapabilitiesTextInputValue(this.webRTCommClientConfiguration.sip.sipUserAgentCapabilities);
    this.view.setSipDomainTextInputValue(this.webRTCommClientConfiguration.sip.sipDomain);
    this.view.setSipDisplayNameTextInputValue(this.webRTCommClientConfiguration.sip.sipDisplayName);
    if(this.sipUserName!='' && this.sipUserName!=undefined)
        this.view.setSipUserNameTextInputValue(this.sipUserName);
    //this.view.setSipUserNameTextInputValue(this.webRTCommClientConfiguration.sip.sipUserName);
    this.view.setSipLoginTextInputValue(this.webRTCommClientConfiguration.sip.sipLogin);
    this.view.setSipPasswordTextInputValue(this.webRTCommClientConfiguration.sip.sipPassword);
    if(this.sipContactUri!='' && this.sipContactUri!=undefined)
        this.view.setSipContactUriTextInputValue(this.sipContactUri);
    this.view.setAudioCodecsFilterTextInputValue(WebRTCController.prototype.DEFAULT_AUDIO_CODECS_FILTER);
    this.view.setVideoCodecsFilterTextInputValue(WebRTCController.prototype.DEFAULT_VIDEO_CODECS_FILTER);
    this.view.setLocalVideoFormatTextInputValue(WebRTCController.prototype.DEFAULT_LOCAL_VIDEO_FORMAT)
    
    // Get local user media
    try
    {
        this.getLocalUserMedia(WebRTCController.prototype.DEFAULT_LOCAL_VIDEO_FORMAT)
    }
    catch(exception)
    {
        console.error("WebRTCController:onLoadEventHandler(): caught exception: "+exception);
        modal_alert("WebRTCController:onLoadEventHandler(): caught exception: "+exception);
    }   
}
  
WebRTCController.prototype.getLocalUserMedia=function(videoContraints){

    videoConstraints='{ mandatory: {maxWidth: ' + g_maxWidth + ', maxHeight:' + g_maxHeight + '}}';

    console.debug ("WebRTCController:getLocalUserMedia():videoConstraints="+JSON.stringify(videoConstraints));

    var that = this;
    this.view.stopLocalVideo();
    if(this.localAudioVideoMediaStream) this.localAudioVideoMediaStream.stop();
    if(navigator.getUserMedia)
    {
        // Google Chrome user agent
        navigator.getUserMedia({
            audio:true, 
            video: g_enableVideo ? JSON.parse(videoContraints) : false,
        }, function(localMediaStream) {
            that.onGetUserMediaSuccessEventHandler(localMediaStream);
        }, function(error) {
            that.onGetUserMediaErrorEventHandler(error);
        });
    }
    else
    {
        console.error("WebRTCController:onLoadEventHandler(): navigator doesn't implemement getUserMedia API")
        modal_alert("WebRTCController:onLoadEventHandler(): navigator doesn't implemement getUserMedia API")     
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
        this.localAudioVideoMediaStream.onended = function() {
            console.debug("this.localAudioVideoMediaStream.onended")
        }
        var audioTracks = undefined;
        if(this.localAudioVideoMediaStream.audioTracks) audioTracks=this.localAudioVideoMediaStream.audioTracks;
        else if(this.localAudioVideoMediaStream.getAudioTracks) audioTracks=this.localAudioVideoMediaStream.getAudioTracks();
        if(audioTracks)
        {
            console.debug("WebRTCController:onWebkitGetUserMediaSuccessEventHandler(): audioTracks="+JSON.stringify(audioTracks));
            for(var i=0; i<audioTracks.length;i++)
            {
                audioTracks[i].onmute = function() {
                    console.debug("videoTracks[i].onmute")
                };
                audioTracks[i].onunmute = function() {
                    console.debug("audioTracks[i].onunmute")
                }
                audioTracks[i].onended = function() {
                    console.debug("audioTracks[i].onended")
                }
            }             
            audioTracks.onmute = function() {
                console.debug("audioTracks.onmute")
            };
            audioTracks.onunmute = function() {
                console.debug("audioTracks.onunmute")
            }
            audioTracks.onended = function() {
                console.debug("audioTracks.onended")
            } 
        }
        else
        {
            console.debug("MediaStream Track  API not supported");
        }
        
        var videoTracks = undefined;
        if(this.localAudioVideoMediaStream.videoTracks) videoTracks=this.localAudioVideoMediaStream.videoTracks;
        else if(this.localAudioVideoMediaStream.getVideoTracks) videoTracks=this.localAudioVideoMediaStream.getVideoTracks();
        if(videoTracks)
        {
            console.debug("WebRTCController:onWebkitGetUserMediaSuccessEventHandler(): videoTracks="+JSON.stringify(videoTracks));
            for(var i=0; i<videoTracks.length;i++)
            {
                videoTracks[i].onmute = function() {
                    console.debug("videoTracks[i].onmute")
                };
                videoTracks[i].onunmute = function() {
                    console.debug("videoTracks[i].onunmute")
                }
                videoTracks[i].onended = function() {
                    console.debug("videoTracks[i].onended")
                }
            }
            videoTracks.onmute = function() {
                console.debug("videoTracks.onmute")
            };
            videoTracks.onunmute = function() {
                console.debug("videoTracks.onunmute")
            }
            videoTracks.onended = function() {
                console.debug("videoTracks.onended")
            }
        }
        
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
  
/**
 * on connect event handler
 */
WebRTCController.prototype.onChangeLocalVideoFormatViewEventHandler=function()
{
    console.debug ("WebRTCController:onChangeLocalVideoFormatViewEventHandler()");  
    // Get local user media
    try
    {
        this.getLocalUserMedia(this.view.getLocalVideoFormatTextInputValue());
    }
    catch(exception)
    {
        console.error("WebRTCController:onChangeLocalVideoFormatViewEventHandler(): caught exception: "+exception);
        modal_alert("WebRTCController:onChangeLocalVideoFormatViewEventHandler(): caught exception: "+exception);
    }   
}
/**
 * on connect event handler
 */ 
WebRTCController.prototype.onClickConnectButtonViewEventHandler=function()
{
    console.debug ("WebRTCController:onClickConnectButtonViewEventHandler()"); 
    if(this.webRTCommClient != undefined)
    {
        try
        {
	    if(this.view.getStunServerTextInputValue() != null)
            {
                this.webRTCommClientConfiguration.RTCPeerConnection.stunServer= this.view.getStunServerTextInputValue();
            }
            else 
            {
                this.webRTCommClientConfiguration.RTCPeerConnection.stunServer=undefined;
            }
            if(this.view.getTurnServerTextInputValue() != null)
            {
                this.webRTCommClientConfiguration.RTCPeerConnection.turnServer= this.view.getTurnServerTextInputValue();
                this.webRTCommClientConfiguration.RTCPeerConnection.turnLogin= this.view.getTurnLoginTextInputValue();
                this.webRTCommClientConfiguration.RTCPeerConnection.turnPassword= this.view.getTurnPasswordTextInputValue();
            }
            else  
            { 
                this.webRTCommClientConfiguration.RTCPeerConnection.turnServer= undefined;
                this.webRTCommClientConfiguration.RTCPeerConnection.turnLogin= undefined;
                this.webRTCommClientConfiguration.RTCPeerConnection.turnPassword= undefined;
            }
           //alert('turn:'+ this.webRTCommClientConfiguration.RTCPeerConnection.turnServer + ',' + this.webRTCommClientConfiguration.RTCPeerConnection.turnLogin);
           //this.webRTCommClientConfiguration.RTCPeerConnection.forceTurnMediaRelay=this.view.getForceTurnMediaRelayValue();
            //this.webRTCommClientConfiguration.RTCPeerConnection.dtlsSrtpKeyAgreement=this.view.getDtlsSrtpKeyAgreementValue();
            this.webRTCommClientConfiguration.sip.sipOutboundProxy = this.view.getSipOutboundProxyTextInputValue();
            //this.webRTCommClientConfiguration.sip.sipUserAgent = this.view.getSipUserAgentTextInputValue(); 
            //this.webRTCommClientConfiguration.sip.sipUriContactParameters = this.view.getSipUriContactParametersTextInputValue();
            //this.webRTCommClientConfiguration.sip.sipUserAgentCapabilities = this.view.getSipUserAgentCapabilitiesTextInputValue();
            this.webRTCommClientConfiguration.sip.sipDomain = this.view.getSipDomainTextInputValue();
            this.webRTCommClientConfiguration.sip.sipDisplayName= this.view.getSipDisplayNameTextInputValue();
            this.webRTCommClientConfiguration.sip.sipUserName = this.view.getSipUserNameTextInputValue();
            //this.webRTCommClientConfiguration.sip.sipLogin = this.view.getSipLoginTextInputValue();
            this.webRTCommClientConfiguration.sip.sipLogin = this.webRTCommClientConfiguration.sip.sipUserName;
            this.webRTCommClientConfiguration.sip.sipPassword = this.view.getSipPasswordTextInputValue();
            //this.webRTCommClientConfiguration.sip.sipRegisterMode = this.view.getSipRegisterModeValue();
            this.webRTCommClient.open(this.webRTCommClientConfiguration); 
            this.view.disableConnectButton();
        }
        catch(exception)
        {
            modal_alert("Connection has failed, reason:"+exception)  
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
    if(this.webRTCommClient != undefined)
    {
        try
        {
            this.webRTCommClient.close();  
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
    if(this.webRTCommCall == undefined)
    {
        try
        {
            var callConfiguration = {
                displayName:this.DEFAULT_SIP_DISPLAY_NAME,
                localMediaStream: this.localAudioVideoMediaStream,
                audioMediaFlag:true,
                videoMediaFlag:true,
                messageMediaFlag:false,
                audioCodecsFilter:this.view.getAudioCodecsFilterTextInputValue(),
                videoCodecsFilter:this.view.getVideoCodecsFilterTextInputValue()
            }
            this.webRTCommCall = this.webRTCommClient.call(calleePhoneNumber, callConfiguration);
            this.view.disableCallButton();
            this.view.disableDisconnectButton();
            //this.view.enableCancelCallButton();
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
WebRTCController.prototype.onClickCancelCallButtonViewEventHandler=function()
{
    console.debug ("WebRTCController:onClickCancelCallButtonViewEventHandler()"); 
    if(this.webRTCommCall != undefined)
    {
        try
        {
            this.webRTCommCall.close();
            this.view.disableCancelCallButton();
            this.view.stopRinging();
        }
        catch(exception)
        {
            console.error("WebRTCController:onClickCancelCallButtonViewEventHandler(): caught exception:"+exception)  
        }
    }
    else
    {
        console.error("WebRTCController:onClickCancelCallButtonViewEventHandler(): internal error");      
    }
}

/**
 * on call event handler
 */ 
WebRTCController.prototype.onClickEndCallButtonViewEventHandler=function()
{
    console.debug ("WebRTCController:onClickEndCallButtonViewEventHandler()"); 
    if(this.webRTCommCall)
    {
        try
        {
            this.webRTCommCall.close();
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
    if(this.webRTCommCall)
    {
        try
        {
            var callConfiguration = {
                displayName:this.DEFAULT_SIP_DISPLAY_NAME,
                localMediaStream: this.localAudioVideoMediaStream,
                audioMediaFlag:true,
                videoMediaFlag:true,
                messageMediaFlag:false
            }
            this.webRTCommCall.accept(callConfiguration);
            this.view.enableEndCallButton();
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
	if(this.webRTCommCall)
    {
        try
        {
            this.webRTCommCall.reject();
            this.view.enableCallButton();
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
 * on accept event handler
 */ 
WebRTCController.prototype.onClickSendMessageButtonViewEventHandler=function()
{
    console.debug ("WebRTCController:onClickSendMessageButtonViewEventHandler()"); 
    if(this.webRTCommCall)
    {
        try
        {
            var message = document.getElementById("messageTextArea").value;
            this.webRTCommCall.sendMessage(message);
            document.getElementById("messageTextArea").value="";
        }
        catch(exception)
        {
            console.error("WebRTCController:onClickRejectCallButtonViewEventHandler(): caught exception:"+exception); 
            alert("Send message failed:"+exception)
        }
    }
    else
    {
        console.error("WebRTCController:onClickRejectCallButtonViewEventHandler(): internal error");      
    }
}



/**
 * on local audio mute event handler
 */ 
WebRTCController.prototype.onClickMuteLocalAudioButtonViewEventHandler=function(checked)
{
    console.debug ("WebRTCController:onClickMuteLocalAudioButtonViewEventHandler():checked="+checked);
    if(this.webRTCommCall)
    {
        try
        {
            if(checked) this.webRTCommCall.muteLocalAudioMediaStream();
            else this.webRTCommCall.unmuteLocalAudioMediaStream();
        }
        catch(exception)
        {
            console.error("WebRTCController:onClickMuteLocalAudioButtonViewEventHandler(): caught exception:"+exception)  
        }
    }
    else
    {
        console.error("WebRTCController:onClickMuteLocalAudioButtonViewEventHandler(): internal error");      
    } 
}

/**
 * on local video hide event handler
 */ 
WebRTCController.prototype.onClickHideLocalVideoButtonViewEventHandler=function(checked)
{
    console.debug ("WebRTCController:onClickHideLocalVideoButtonViewEventHandler():checked="+checked);
    if(this.webRTCommCall)
    {
        try
        {
            if(checked) this.webRTCommCall.hideLocalVideoMediaStream();
            else this.webRTCommCall.showLocalVideoMediaStream();
        }
        catch(exception)
        {
            console.error("WebRTCController:onClickHideLocalVideoButtonViewEventHandler(): caught exception:"+exception)  
        }
    }
    else
    {
        console.error("WebRTCController:onClickHideLocalVideoButtonViewEventHandler(): internal error");      
    }   
}

/**
 * on remote audio mute event handler
 */ 
WebRTCController.prototype.onClickMuteRemoteAudioButtonViewEventHandler=function(checked)
{
    console.debug ("WebRTCController:onClickMuteRemoteAudioButtonViewEventHandler():checked="+checked);
    if(this.webRTCommCall)
    {
        try
        {
            if(checked) this.webRTCommCall.muteRemoteAudioMediaStream();
            else this.webRTCommCall.unmuteRemoteAudioMediaStream();
        }
        catch(exception)
        {
            console.error("WebRTCController:onClickMuteRemoteAudioButtonViewEventHandler(): caught exception:"+exception)  
        }
    }
    else
    {
        console.error("WebRTCController:onClickMuteRemoteAudioButtonViewEventHandler(): internal error");      
    } 
}

/**
 * on remote video mute event handler
 */ 
WebRTCController.prototype.onClickHideRemoteVideoButtonViewEventHandler=function(checked)
{
    console.debug ("WebRTCController:onClickHideRemoteVideoButtonViewEventHandler():checked="+checked);
    if(this.webRTCommCall)
    {
        try
        {
            if(checked) this.webRTCommCall.hideRemoteVideoMediaStream();
            else this.webRTCommCall.showRemoteVideoMediaStream();
        }
        catch(exception)
        {
            console.error("WebRTCController:onClickHideRemoteVideoButtonViewEventHandler(): caught exception:"+exception)  
        }
    }
    else
    {
        console.error("WebRTCController:onClickHideRemoteVideoButtonViewEventHandler(): internal error");      
    }   
}

/**
 * on remote video mute event handler
 */ 
WebRTCController.prototype.onClickStopVideoStreamButtonViewEventHandler=function(checked)
{
    console.debug ("WebRTCController:onClickStopVideoStreamButtonViewEventHandler():checked="+checked);
    var videoTracks = undefined;
    if(this.localAudioVideoMediaStream.videoTracks) videoTracks=this.localAudioVideoMediaStream.videoTracks;
    else if(this.localAudioVideoMediaStream.getVideoTracks) videoTracks=this.localAudioVideoMediaStream.getVideoTracks();
    if(videoTracks)
    {
        for(var i=0; i<videoTracks.length;i++)
        {
            this.localAudioVideoMediaStream.removeTrack(videoTracks[i]);
        }                  
    }  
    if(this.webRTCommCall)
    {
        try
        {
            //this.webRTCommCall.stopVideoMediaStream();
            var videoTracks = undefined;
            if(this.localAudioVideoMediaStream.videoTracks) videoTracks=this.localAudioVideoMediaStream.videoTracks;
            else if(this.localAudioVideoMediaStream.getVideoTracks) videoTracks=this.localAudioVideoMediaStream.getVideoTracks();
            if(videoTracks)
            {
                for(var i=0; i<videoTracks.length;i++)
                {
                    this.localAudioVideoMediaStream.removeTrack(videoTracks[i]);
                }                  
            }  
        }
        catch(exception)
        {
            console.error("WebRTCController:onClickStopVideoStreamButtonViewEventHandler(): caught exception:"+exception)  
        }
    }
    else
    {
        console.error("WebRTCController:onClickStopVideoStreamButtonViewEventHandler(): internal error");      
    }   
}

/**
  * Implementation of the WebRtcCommClient listener interface
  */
WebRTCController.prototype.onWebRTCommClientOpenedEvent=function()
{
    console.debug ("WebRTCController:onWebRTCommClientOpenedEvent()");
    //Enabled button DISCONNECT, CALL diable CONNECT and BYE
    this.view.enableDisconnectButton();
    this.view.enableCallButton();
    this.view.enableVideoCheckbox();
    this.view.setDisplayMessage(null);
    this.view.disableConnectButton();
    this.view.disableEndCallButton();
	//this.view.disableCancelCallButton();
    //this.view.disableSendMessageButton();
    this.view.ng_createConfNumber();
    is_registered=true;
    modal_alert("Succesfully Registered"); 
}
    
WebRTCController.prototype.onWebRTCommClientOpenErrorEvent=function(error)
{
    console.debug ("WebRTCController:onWebRTCommClientOpenErrorEvent():error:"+error); 
    this.view.enableConnectButton();
    this.view.disableDisconnectButton();
    this.view.disableCallButton();
    this.view.enableVideoCheckbox();
    this.view.setDisplayMessage(null);
    this.view.disableEndCallButton();
    //this.view.disableCancelCallButton();
    //this.view.disableSendMessageButton();
    this.webRTCommCall=undefined;
    if(!is_registered)
        modal_alert("Failed to register.  <br>Please check the the NGVX Server address.");
    else
        modal_alert("Connection to the NGVX server has failed.");
} 
    
/**
 * Implementation of the WebRtcCommClient listener interface
 */
WebRTCController.prototype.onWebRTCommClientClosedEvent=function()
{
    console.debug ("WebRTCController:onWebRTCommClientClosedEvent()"); 
    //Enabled button CONNECT, disable DISCONECT, CALL, BYE
    this.view.enableConnectButton();
    this.view.disableDisconnectButton();
    this.view.disableCallButton();
    this.view.enableVideoCheckbox();
    this.view.setDisplayMessage(null);
    this.view.disableEndCallButton();
    //this.view.disableCancelCallButton();
    //this.view.disableSendMessageButton();
    this.webRTCommCall=undefined;
    if(is_registered)
        modal_alert("You have unregistered");
    else
        modal_alert("Failed to register.  <br>Please check the the NGVX Server address.");
    is_registered=false;
}
    
/**
 * Implementation of the webRTCommCall listener interface
 */
WebRTCController.prototype.onWebRTCommCallClosedEvent=function(webRTCommCall)
{
    console.debug ("WebRTCController:onWebRTCommCallClosedEvent(): webRTCommCall.getId()="+webRTCommCall.getId()); 

    //Enabled button DISCONECT, CALL
    this.view.enableCallButton();
    this.view.enableVideoCheckbox();
    this.view.setDisplayMessage(null);
    this.view.enableDisconnectButton();
    this.view.disableEndCallButton();
    this.view.disableConnectButton();
    this.view.stopRemoteVideo();
    this.view.hideRemoteVideo();
    //this.view.disableCancelCallButton();
    //this.view.disableSendMessageButton();
    this.webRTCommCall=undefined;
    this.view.ng_onLeaveConf();
    if(is_connected)
       modal_alert("You have disconnected from the call.");
    else
       modal_alert("Failed to join the call.");
    is_connected=false
    
}
   
   
/**
 * Implementation of the webRTCommCall listener interface
 */
WebRTCController.prototype.onWebRTCommCallOpenedEvent=function(webRTCommCall)
{
    console.debug ("WebRTCController:onWebRTCommCallOpenedEvent(): webRTCommCall.getId()="+webRTCommCall.getId()); 
   
    this.view.stopRinging();
    this.view.disableCallButton();
    this.view.disableVideoCheckbox();
    this.view.setDisplayMessage(null);
    this.view.disableDisconnectButton();
    this.view.enableEndCallButton();
    this.view.disableDisconnectButton();
    this.view.disableConnectButton();
    //this.view.disableCancelCallButton();
    //this.view.enableSendMessageButton();
    if(webRTCommCall.getRemoteBundledAudioVideoMediaStream())
    {
        this.view.showRemoteVideo();
        this.view.playRemoteVideo(webRTCommCall.getRemoteBundledAudioVideoMediaStream());
    }
    else
    {
        /*if(webRTCommCall.getRemoteAudioMediaStream())
        {
            this.view.playRemoteAudio(webRTCommCall.getRemoteAudioMediaStream());
        } */
        if(webRTCommCall.getRemoteVideoMediaStream())
        {
            this.view.showRemoteVideo();
            this.view.playRemoteVideo(webRTCommCall.getRemoteVideoMediaStream());
        } 
    }
    
    this.view.ng_onJoinConf();
    modal_alert("Connected to the call!");
    is_connected=true
}

/**
 * Implementation of the webRTCommCall listener interface
 */
WebRTCController.prototype.onWebRTCommCallInProgressEvent=function(webRTCommCall)
{
    console.debug ("WebRTCController:onWebRTCommCallInProgressEvent(): webRTCommCall.getId()="+webRTCommCall.getId()); 

    modal_alert("Communication in progress"); 
}


/**
 * Implementation of the webRTCommCall listener interface
 */
WebRTCController.prototype.onWebRTCommCallOpenErrorEvent=function(webRTCommCall, error)
{
    console.debug ("WebRTCController:onWebRTCommCallOpenErrorEvent(): webRTCommCall.getId()="+webRTCommCall.getId()); 

    //Enabled button DISCONECT, CALL
    this.view.enableCallButton();
    this.view.enableVideoCheckbox();
    this.view.setDisplayMessage(null);
    this.view.enableDisconnectButton();
    this.view.disableEndCallButton();
    this.view.disableConnectButton();
    this.view.hideRemoteVideo();
    this.view.stopRemoteVideo();
    this.view.stopRinging();
    //this.view.disableCancelCallButton();
    //this.view.disableSendMessageButton();
    this.webRTCommCall=undefined;
    modal_alert("Communication failed: error:"+error); 
}

/**
 * Implementation of the webRTCommCall listener interface
 */
WebRTCController.prototype.onWebRTCommCallRingingEvent=function(webRTCommCall)
{
    console.debug ("WebRTCController:onWebRTCommCallRingingEvent(): webRTCommCall.getId()="+webRTCommCall.getId()); 
    this.webRTCommCall=webRTCommCall;
    this.view.playRinging();
    this.view.disableCallButton();
    this.view.disableVideoCheckbox();
    this.view.setDisplayMessage(null);
    this.view.disableDisconnectButton();
    this.view.disableEndCallButton();
    this.view.disableConnectButton();
    //this.view.disableSendMessageButton();
    //this.view.disableCancelCallButton();
    show_desktop_notification("Incoming Call from " + webRTCommCall.getCallerPhoneNumber());
    $("#call_message").html("<p>Incoming Call from " + webRTCommCall.getCallerPhoneNumber() +"</p>");
     $('#callModal').modal(); 
}

/**
 * Implementation of the webRTCommCall listener interface
 */
WebRTCController.prototype.onWebRTCommCallRingingBackEvent=function(webRTCommCall)
{
    console.debug ("WebRTCController:onWebRTCommCallRingingBackEvent(): webRTCommCall.getId()="+webRTCommCall.getId()); 
    this.view.playRinging();
    this.view.disableCallButton();
    this.view.disableVideoCheckbox();
    this.view.setDisplayMessage(null);
    this.view.disableDisconnectButton();
    this.view.disableEndCallButton();
    //this.view.disableSendMessageButton();
    //this.view.enableCancelCallButton();
    this.view.disableConnectButton();
}

/**
 * Implementation of the WebRTCommCall listener interface
 */
WebRTCController.prototype.onWebRTCommCallHangupEvent=function(webRTCommCall)
{
    console.debug ("WebRTCController:onWebRTCommCallHangupEvent(): webRTCommCall.getId()="+webRTCommCall.getId()); 
    //Enabled button DISCONECT, CALL
    this.view.enableCallButton();
    this.view.enableVideoCheckbox();
    this.view.setDisplayMessage(null);
    this.view.enableDisconnectButton();
    //this.view.disableRejectCallButton();
    //this.view.disableAcceptCallButton();
    this.view.disableEndCallButton();
    //this.view.disableCancelCallButton();
    this.view.disableConnectButton();
    //this.view.disableSendMessageButton();    
    this.view.stopRemoteVideo();
    //this.view.stopRemoteAudio();
    this.view.stopRinging();
    this.view.hideRemoteVideo();
    this.webRTCommCall=undefined;
    
    if(webRTCommCall.getCallerPhoneNumber())
        modal_alert("Communication closed by "+webRTCommCall.getCallerPhoneNumber());
    else 
        modal_alert("Communication close by "+webRTCommCall.getCalleePhoneNumber());
}

/**
 * Implementation of the WebRTCommCall listener interface
 */
WebRTCController.prototype.onWebRTCommCallMessageEvent=function(webRTCommCall, message)
{
    console.debug ("WebRTCController:onWebRTCommCallMessageEvent(): webRTCommCall.getId()="+webRTCommCall.getId()); 
    if(webRTCommCall.isIncoming()) alert("Message from "+webRTCommCall.getCallerPhoneNumber()+":"+message);
    else alert("Message from "+webRTCommCall.getCalleePhoneNumber()+":"+message);
}


/**
 * Message event
 * @public
 * @param {WebRTCommCall} webRTCommCall source WebRTCommCall object
 */
WebRTCommCallEventListenerInterface.prototype.onWebRTCommCallMessageEvent= function(webRTCommCall, message) {
    throw "WebRTCommCallEventListenerInterface:onWebRTCommCallMessageEvent(): not implemented;";   
}




