/**
 *  Copyright 2014 OpenVCX openvcx@gmail.com
 *
 *  You may not use this file except in compliance with the OpenVCX License.

 *  The OpenVCX License is based on the Apache Version 2.0 License with
 *  additional credit attribution clauses mentioned in section 4 (e) and
 *  4 (f).
 *
 *  4 (e) Redistributions in source or binary form must reproduce the
 *        aforementioned copyright notice, list of conditions and any
 *        disclaimers in the documentation and/or other materials provided
 *        with the distribution.
 *
 *    (f) All advertising materials mentioning features or use of this
 *        software must display the following acknowledgement:
 *        "This product includes software from OpenVCX".
 *
 *  You may obtain a copy of the Apache License, Version 2.0  at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

package com.openvcx.webcall;

import com.openvcx.util.*;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.InputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileNotFoundException;
import java.io.OutputStream;
import java.math.BigInteger;
import java.nio.charset.Charset;
import java.security.SecureRandom;
import java.util.Iterator;
import java.util.Properties;
import java.util.Random;

import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

//import org.apache.commons.io.FileUtils;

import org.apache.log4j.Logger;


/**
 * 
 */
public class ConferenceCreateServlet extends HttpServlet {

    private final Logger logger = Logger.getLogger(getClass());

    private static final String CONFIG_CONFERENCE_DEF                      = "conference.definition.dir";
    private static final String CONFIG_CONFERENCE_TEMPLATE                 = "conference.definition.template";
    private static final String CONFIG_CONFERENCE_PROVISION_AUTO           = "conference.definition.provision.auto";
    private static final String CONFIG_CONFERENCE_TEMP_DIR                 = "conference.temp.dir";
    private static final String CONFIG_MEDIAPORTAL_LISTENER_ADDRESS        = "mediaportal.listener.address";
    private static final String CONFIG_MEDIAPORTAL_LISTENER_ADDRESS_LEGACY = "mediaportal.listener.address";
    private static final String CONFERNCE_CONFIG_DIR                       = "conferences/";
    private static final int CONFERENCE_NUMBER_LENGTH                      = 8;

    private static final String CONF_DEF_FILE_SUFFIX                       = ".conference";
    private static final String CONFERENCE_TEMP_DIR                        = "temp";
    private static final String COOKIE_NUMBER_KEY                          = "sn";
    private static final int SECONDS_IN_DAY                                = 86400;

    private String m_confDefDir;
    private String m_confDefDirAuto;
    private String m_tempDir;
    private String m_mediaPortalAddress;
    private boolean m_bAutoProvision = false;
    private Configuration m_config = null;
    private SecureRandom m_rand;

    /**
     * <i>javax.Servlet</i> overridden initializer method
     */
    @Override
    public void init(ServletConfig c) throws ServletException {
        logger.debug("init start");
        super.init(c);

        try {

            //
            // Load the config file defined in web.xml 'sip-ngconference.conf'
            //
            String configFilePath = getServletContext().getInitParameter("config");
            m_config = new Configuration(configFilePath);

            m_confDefDir = m_config.getString(CONFIG_CONFERENCE_DEF, CONFERNCE_CONFIG_DIR);
            if(m_confDefDir.charAt(m_confDefDir.length() - 1) != File.separatorChar) {
                m_confDefDir += File.separatorChar;
            }
            m_confDefDirAuto = m_confDefDir + "auto/"; 

            m_tempDir = m_config.getString(CONFIG_CONFERENCE_TEMP_DIR, CONFERENCE_TEMP_DIR);
            if(m_tempDir.charAt(m_tempDir.length() - 1) != File.separatorChar) {
                m_tempDir += File.separatorChar;
            }

            if(null == (m_mediaPortalAddress = m_config.getString(CONFIG_MEDIAPORTAL_LISTENER_ADDRESS, null))) {
                // Try to lookup the legacy naming convention if the user has somehow retained
                // an old sip-ngconference.conf
                m_mediaPortalAddress = m_config.getString(CONFIG_MEDIAPORTAL_LISTENER_ADDRESS_LEGACY, null);
            }
            m_bAutoProvision = m_config.getBoolean(CONFIG_CONFERENCE_PROVISION_AUTO, m_bAutoProvision);
 
            logger.info("confDefDirAuto: '" + m_confDefDirAuto + "', tempDir: '" + m_tempDir + 
                        "', mediaPortal: '" + m_mediaPortalAddress + ", provision.auto: " + m_bAutoProvision);

            m_rand = new SecureRandom();

        } catch(Exception e) {
            LogUtil.printError(logger, "init: ", e);
            throw new ServletException(e);
        }

        logger.debug("init done");

    }

    /**
     * <i>javax.Servlet</i> overridden cleanup method 
     */
    @Override
    public void destroy() {
        logger.debug("destroy");
        super.destroy();
    }

    private void copyFile(String strFrom, String strTo, boolean bAppend) 
                     throws FileNotFoundException, IOException {
        InputStream in = new FileInputStream(new File(strFrom));
        OutputStream out = new FileOutputStream(new File(strTo), bAppend);

        byte[] buf = new byte[4096];
        int len;
        while ((len = in.read(buf)) > 0){
            out.write(buf, 0, len);
        }
        in.close();
        out.close();
    }

    private synchronized String createConferenceNumber() {
        int index = 0;
        String strResult = "";

        while(index < CONFERENCE_NUMBER_LENGTH) {
            BigInteger bi = new BigInteger(4 * 8, m_rand);
            String str = bi.toString();
            int end = Math.min(CONFERENCE_NUMBER_LENGTH - index, str.length());
            str = str.substring(0, end);
            strResult += str;
            index += str.length();
        }

        return strResult;
    }

    private boolean isConferenceDefinitionExists(String number) {

        String pathDef = m_confDefDir + number + CONF_DEF_FILE_SUFFIX;
        String pathDefAuto = m_confDefDirAuto + number + CONF_DEF_FILE_SUFFIX;
        if(new File(pathDef).canRead() || new File(pathDefAuto).canRead()) {
            return true;
        }
        return false;
    }

    /**
     * <p>Creates a conference definition file according to the <i>strRequestedNumber</i> </p>
     * <p>If <i>strRequestedNumber</i> is null then a random SIP URI phone number will be automatically created.</p>
     * <p>A conference definition file will be created for the SIP URI phone number</p>
     * @param strRequestedNumber The requested SIP URI phone number or null
     */
    private String createConferenceDefinition(String strRequestedNumber) throws IOException {

        int indexTry = 0;
        String templateFile = m_config.getString(CONFIG_CONFERENCE_TEMPLATE);
        String templatePath = m_confDefDir + templateFile;
        String outputCreatePath = null;
        String strOutputNumber = null;

        if(null != strRequestedNumber && strRequestedNumber.length() > 0) {

            if(strRequestedNumber.length() < CONFERENCE_NUMBER_LENGTH) {
                // Treat it as an invalid / non-secure requested number
                logger.warn("Conference definition requested number length is too short: '" + 
                            strRequestedNumber + "'");
                return null;
            }
            strOutputNumber = strRequestedNumber;

            if(true == isConferenceDefinitionExists(strRequestedNumber)) {
                //
                // The requested number conference definition file already exists
                //
                outputCreatePath = null;
            } else {
                outputCreatePath = m_confDefDirAuto + strRequestedNumber + CONF_DEF_FILE_SUFFIX;
            }

        }

        if(null == strOutputNumber && null == outputCreatePath) {
            // 
            // Create a unique pseudo-random SIP URI phone number 
            //
            do {
                strOutputNumber = createConferenceNumber();
                if(true == isConferenceDefinitionExists(strOutputNumber)) {
                    //TODO: check conflict at root conference/
                    if(indexTry++ > 20) {
                        throw new IOException("Failed to create unique conference definition file after " + 
                                              indexTry + "attempts");
                    }
                    continue;
                }
            } while(false);

            outputCreatePath = m_confDefDirAuto + strOutputNumber + CONF_DEF_FILE_SUFFIX;
        }

        if(true == m_bAutoProvision) {
            if(null != outputCreatePath) {
                copyFile(templatePath, outputCreatePath, false);
                logger.debug("Copied auto-provisioned conference template: '" + templatePath + "' -> '" + 
                             outputCreatePath + "'");
            } else {
                logger.debug("Using existing auto-provisioned conference definition number: '" + 
                             strOutputNumber + "'");
            }
        } else {
            logger.debug("Assigned non-provisioned conference number: " + strOutputNumber);
        }

        return strOutputNumber;
    }

    private void redirect(HttpServletResponse response, String location) throws IOException {
        response.setStatus(302);
        response.setHeader("Location", location);
        response.setHeader("Connection", "close");

        PrintWriter     out;
        response.setContentType("text/html");
        out = response.getWriter();
        out.println("<html><head><meta http-equiv=\"refresh\"content=\"1; URL=" + location + "\"><head>></html>");
        out.close();
    }

    private void redirect2(HttpServletResponse response, String location) throws IOException {
        PrintWriter     out;
        response.setContentType("text/html");
        out = response.getWriter();

        // Can't embed media web portal link into iframe due to browser cross-site scripting restrictions 
        // per different domain / port.
        StringBuffer sb = new StringBuffer(
                  "<html><head><title>test</title></head><body>" +
                  "<div id=\"mplayerdiv\" style=\"width:100%; height:100%; margin: 0px 0px 0px 0px; " +
                  "padding: 0px 0px 0px 0px;\">" +
                  "<iframe id=\"mplayerframe\" name=\"mplayerframe\" src=\"")
                  .append(location)
                  .append("\" frameborder=\"0\" width=\"100%\" height=\"100%\">" +
                  "<p>Your browser does not support iframes. <a href=\"")
                  .append(location).append("\">").append(location).append("</a></p></iframe>" +
                  "</div></body></html>");

        out.println(sb.toString());
        out.close();

    }

    private String getUriDirSegment(String uri, int segmentIndex) {

        if(null == uri || 0 == uri.length()) {
            return null;
        }

        while('/' == uri.charAt(0)) {
            uri = uri.substring(1);
        }

        String [] uris = uri.split("/");

        if(segmentIndex < uris.length) {
            return uris[segmentIndex];
        }
        return null;
    }

    private String getUriAfterSegment(String uri, int segmentIndex) {
        StringBuffer remaining = new StringBuffer();

        while('/' == uri.charAt(0)) {
            uri = uri.substring(1);
        }

        String [] uris = uri.split("/");

        while(segmentIndex < uris.length) {
            remaining.append('/' + uris[segmentIndex++]);
        }
        return remaining.toString();
    }

    /**
     * <p>Lookup a client conference number stored in a cookie. If no phone number is provided by the client a random SIP URI phone number is automatically generated.</p>
     * <p>A conference definition template file is used to create the conference definition for the phone number.</p>
     * <p>The auto-assigned phone number is then stored in a cookie and returned to the client.</p>
     * @param out standard output Output writer
     * @param request The HTTP request object
     * @param response The HTTP response object
     */
    private boolean doCreateNumber(PrintWriter out, HttpServletRequest request, HttpServletResponse response) 
                        throws IOException {
        String strOutputNumber = null;

        Cookie[] arrCookies = request.getCookies();
        if(null != arrCookies) {
            for(Cookie cookie : arrCookies) {
                //logger.debug("cookie name: " + cookie.getName() + ", path: " + cookie.getPath() + ", domain: " + cookie.getDomain() + ", maxAge: " + cookie.getMaxAge() + ", value: " + cookie.getValue());
                if(COOKIE_NUMBER_KEY.equals(cookie.getName())) {
                    if(null != (strOutputNumber = cookie.getValue()) && strOutputNumber.length() == 0) {
                        strOutputNumber = null;
                    }
                    logger.debug("Using cookie stored conference output number: '" + strOutputNumber + "'.");
                    break;
                }
            } 
        }

        strOutputNumber = createConferenceDefinition(strOutputNumber);

        if(null != strOutputNumber) {

            int cookieAgeDays = 7;
            Cookie cookie = new Cookie(COOKIE_NUMBER_KEY, strOutputNumber);
            cookie.setMaxAge(cookieAgeDays * SECONDS_IN_DAY);
            cookie.setPath("/" + getUriDirSegment(request.getRequestURI(), 0) + "/");
            logger.debug("Setting cookie " + COOKIE_NUMBER_KEY + "=" + strOutputNumber);
            response.addCookie(cookie);
            out.println("number=" + strOutputNumber);
        }

        return true;
    }


    private String getLiveAddress(File file) throws IOException, FileNotFoundException {

        BufferedReader br = new BufferedReader(new InputStreamReader((new FileInputStream(file)), 
                                              Charset.forName("UTF-8")));

        String connectAddress = null;
        String line;
        while ((line = br.readLine()) != null) {
            connectAddress = line;
            if(connectAddress.length() < 7 || false == "http:".equalsIgnoreCase(connectAddress.substring(0, 5))) {
                connectAddress = "http://" + connectAddress;
            }
            logger.debug("Found conference live address mapping file '" + file.getAbsolutePath() + "' -> '" + 
                         connectAddress + "'");
            break;
        }
 
        br.close();

        return connectAddress;
    }

    private String getMediaPortalAddress(String dir, String file) { 
        StringBuffer connectAddress;
        boolean haveQ = false;

        connectAddress = new StringBuffer("http://" +  m_mediaPortalAddress);

        if(null != dir && dir.length() > 0) {
            connectAddress.append((false == haveQ ? "?" : "") + "dir=" + dir + "&");
            haveQ = true;
        }
        if(null != file && file.length() > 0) {
            connectAddress.append((false == haveQ ? "?" : "") + "file=" + file);
            haveQ = true;
        }

        return connectAddress.toString();
    }

    private String getRecordAddress(File file) throws IOException, FileNotFoundException {

        if(null == m_mediaPortalAddress ||  m_mediaPortalAddress.length() == 0) {
            logger.warn(CONFIG_MEDIAPORTAL_LISTENER_ADDRESS + " (Media Portal) Address not set in configuration");
            return null; 
        }

        BufferedReader br = new BufferedReader(new InputStreamReader((new FileInputStream(file)),
                                              Charset.forName("UTF-8")));

        String connectAddress = null;
        String line;
        while ((line = br.readLine()) != null) {

            String recordingDir = getUriDirSegment(line.trim(), 0); // path relative to media portal root dir
            //String recordingPath = getUriAfterSegment(line, 1); // path relative to media portal root dir
            String recordingPath = line.trim(); // path relative to media portal root dir

            connectAddress = getMediaPortalAddress(recordingDir, recordingPath);
            
            logger.debug("Found conference static address mapping file '" + file.getAbsolutePath() + "' -> '" + 
                         connectAddress + "'");
            break;
        }

        br.close();

        return connectAddress;
    }

    private boolean redirectLiveMediaLink(PrintWriter out, String number, HttpServletResponse response) 
                        throws FileNotFoundException, IOException {
        String path = null;
        try {
            String connectAddress = null;
            path = m_tempDir + number + ".live";
            File file = new File(path);
            if(true == file.canRead() && null != (connectAddress = getLiveAddress(file))) {

                String link = connectAddress + "/live";
                link += "?id=" + number;
                logger.info("Redirecting link request to live '" + link + "' obtained from '" + path + "'");
                redirect(response, link);
                return true;
            }

        } catch(FileNotFoundException e) {
            //LogUtil.printError(logger, "Conference address mapping file path: '" + path + "' not found", e);
            logger.warn("Conference live address mapping file path '" + path + "' not found.");
        }

        return false;
    }

    private boolean redirectRecordedMediaLink(PrintWriter out, String number, HttpServletResponse response) 
                        throws FileNotFoundException, IOException {
        String path = null;
        try {
            String connectAddress = null;
            path = m_tempDir + number + ".record";
            File file = new File(path);
            if(file.canRead() && null != (connectAddress = getRecordAddress(file))) {

                String link = connectAddress;
                logger.info("Redirecting link request to recorded '" + link + "' obtained from '" + path + "'");
                redirect(response, link);
                return true;
            }

        } catch(FileNotFoundException e) {
            //LogUtil.printError(logger, "Conference address mapping file path: '" + path + "' not found", e);
            logger.warn("Conference port recorded mapping file path '" + path + "' not found.");
        }

        return false;
    }

    private boolean redirectRecordedListingLink(PrintWriter out, String number, HttpServletResponse response) 
                        throws IOException {

        String link = getMediaPortalAddress(number, null);
        logger.info("Redirecting link request to media listing '" + link + "' obtained for number: '" + 
                     number + "'");
        redirect(response, link);
        return true;
    }

    /**
     * <p>Constructs a hyperlink which can be used to view the live or archived media content of the client.</p>
     * @param out standard output Output writer
     * @param request The HTTP request object
     * @param response The HTTP response object
     */
    private void doGetMediaLink(PrintWriter out, String uri, HttpServletResponse response) 
                        throws FileNotFoundException, IOException {

        String number = getUriDirSegment(uri, 0);
        boolean bProcessed = false;

        try {

            if(null != number && number.length() > 0 && false == number.contains("..")) {

                if(false == (bProcessed = redirectLiveMediaLink(out, number, response)) &&
                   false == (bProcessed = redirectRecordedMediaLink(out, number, response))) {

                    logger.debug("Conference live or recorded address mapping file not found for number: '" + 
                                  number + "'.  Redirecting to media listing");
                    bProcessed = redirectRecordedListingLink(out, number, response);
                    //TODO return media web portal linke regardless if .record is found
                }
            } else {
                logger.error("Conference address mapping request has invalid number: '" + number + "'.");
            }

        } catch(FileNotFoundException e) {
            LogUtil.printError(logger, "Conference address mapping file not found for number: '" + number + "'.", e);
        }

        if(false == bProcessed) {
            response.setStatus(404);
            response.setContentType("text/html");
            response.setHeader("Connection", "close");
            logger.error("Conference address mapping file not found for number: '" + number + "', returning 404.");
        }

    }

    /**
     *
     * <p>HTTP GET method handler</p>
     * <p>The handler services the following URLS:</>
     * <p><i>/createNumber</i> - To create a conference definition file</p>
     * <p>Lookup a client conference number stored in a cookie. If no phone number is provided by the client a random SIP URI phone number is automatically generated.</p>
     * <p>A conference definition template file is used to create the conference definition for the phone number.</p>
     * <p>The auto-assigned phone number is then stored in a cookie and returned to the client.</p>
     * <p><i>/link</i> or <i>/l</i> - </p>
     * <p>Constructs a hyperlink which can be used to view the live or archived media content of the client.</p>
     * @param request The HTTP request object
     * @param response The HTTP response object
     */
    @Override
    public void doGet (HttpServletRequest request, HttpServletResponse response)
        throws ServletException, IOException {

        logger.debug("webcall doGet '" + request.getRequestURI() + "'");

        // Write the output html
        PrintWriter out = response.getWriter();

        try {

            String method = getUriDirSegment(request.getRequestURI(), 2);
            if("createNumber".equalsIgnoreCase(method)) {
                doCreateNumber(out, request, response);
            } else if("link".equalsIgnoreCase(method) || "l".equalsIgnoreCase(method)) {
                doGetMediaLink(out, getUriAfterSegment(request.getRequestURI(), 3), response);
            }

        } catch(Exception e) {
            LogUtil.printError(logger, "doGet: ", e);
        }

        out.close();

    }
}
