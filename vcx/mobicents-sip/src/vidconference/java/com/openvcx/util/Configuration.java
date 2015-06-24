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

package com.openvcx.util;

import java.io.IOException;
import java.io.File;
import java.io.FileInputStream;
import java.lang.StringBuilder;
import java.util.Properties;

import org.apache.log4j.Logger;

/**
 *
 * <p>A Configuration loader and context used for loading configuration elements
 * from a config file. 
 * <p>The config file is a text file containing key value elements.  
 * Value elements are allowed to reference environment variables which will be automatically substituted on key lookup</p>
 * <p>Below is a simple example configuration syntax</p>
 * <p>
 * <i>a=Some text value</i><br>
 * <i>b="Some quoted text value"</i><br>
 * <i>a.b.c=1</i><br>
 * <i>a.b.f=1.234</i><br>
 * <i>x.y.bool1=true</i><br>
 * <i>x.y.bool2=false</i><br>
 * <i>x.y.z1="${SOME_ENVIRONMENT_VARIABLE_NAME}"</i><br>
 * <i>x.y.z2="${SOME_ENVIRONMENT_VARIABLE_NAME}/path/to/file"</i><br>
 * </p>
 *
 */
public class Configuration extends Properties {
    private final Logger m_log = Logger.getLogger(getClass());
    private String m_filePath;
    private File m_file;

    /**
     * Constructor used to initialize this configuration instance from a config file path
     * @throws IOException thrown upon encountering an error parsing the configuration file
     * @param filePath The filepath of the input config file
     */
    public Configuration(String filePath) throws IOException {
        m_filePath = filePath;

        m_log.debug("Processing configuration file: " + m_filePath);
        load(new FileInputStream(new File(m_filePath)));       
        

        m_log.debug("Loaded " + size() + " config entries from " + m_filePath);
        m_log.debug(this.toString()); 
    }

    /**
     * Obtains the filepath which was used to initialize the configuration instance
     * @return The filepath of the input config file which was used to initialize this instance
     */
    public String getFilePath() {
        return m_filePath;
    }

    /**
     * <p>Used to load a property from the config store.</p>
     * <p>This method overrides the base class behavior by performing  environment variable substitution for the stored property values</p>
     */
    public String getProperty(String key) {
        String value = unQuote(super.getProperty(key));

        if(null != value && value.length() > 0 && value.contains("$")) {
            //
            // Handle environment variable substitution
            //
            String envValue;
            StringBuilder sbEnv = null;
            StringBuilder sbIn = new StringBuilder(value);
            StringBuilder sbOut = new StringBuilder();

            for(int index = 0; index < sbIn.length(); index++) {
                char c = sbIn.charAt(index);
                boolean bAppend = true;
                switch(c) {
                    case '$':
                        sbEnv = new StringBuilder();
                    default:

                        if(null != sbEnv && ( ')' == c || '}' == c || File.separatorChar == c)) {

                            if(null != (envValue = lookupEnvVar(sbEnv.toString()))) {
                                sbOut.append(envValue);
                            }

                            sbEnv = null;
                            bAppend = false;
                        }

                        if(null != sbEnv) {
                            sbEnv.append(c);
                        } else if(bAppend) {
                            sbOut.append(c);
                        }

                        break;
                }
            } 
            if(null != sbEnv) {
                if(null != (envValue = lookupEnvVar(sbEnv.toString()))) {
                    sbOut.append(envValue);
                }

            }
            //m_log.debug("key:'" + key + "' -> '" + sbOut.toString() + "'");
            return sbOut.toString();
        } 

        return value; 
    }

    /**
     * <p>Performs lookup of a string config value</p>
     * @param key The unique configuration element key
     * @param dflt The default value to be returned if no element with <i>key</i> is found
     * @return The value stored in the configuration identified by the <i>key</i>
     */
    public String getString(String key, String dflt) {
        String value = getProperty(key);
        if(null == value) {
            value = dflt;
        }
        return value;
    }

    /**
     * <p>Performs lookup of a string config value.  Throws an IOException if the key is not found in the configuration</p>
     * @throws IOException thrown if the key is not found in the configuration
     * @param key The unique configuration element key
     * @return The value stored in the configuration identified by the <i>key</i>
     */
    public String getString(String key) throws IOException {
        String value = getProperty(key);
        if(null == value) {
            errorMessage(key);
        }

        return value;
    }

    /**
     * <p>Performs lookup of an int config value</p>
     * @throws  NumberFormatException thrown if the value cannot be parsed as an int
     * @param key The unique configuration element key
     * @param dflt The default value to be returned if no element with <i>key</i> is found
     * @return The value stored in the configuration identified by the <i>key</i> represented as an int
     */
    public int getInt(String key, int dflt) throws NumberFormatException {
        String value = getProperty(key);
        if(null == value) {
            return dflt;
        }
        return Integer.parseInt(value);
    }

    /**
     * <p>Performs lookup of a int config value.  Throws an IOException if the key is not found in the configuration</p>
     * @throws  IOException thrown if the key is not found in the configuration
     * @throws  NumberFormatException thrown if the value cannot be parsed as an int
     * @param key The unique configuration element key
     * @return The value stored in the configuration identified by the <i>key</i> represented as an int
     */
    public int getInt(String key) throws IOException, NumberFormatException {
        String value = getProperty(key);
        if(null == value) {
            errorMessage(key);
        } 
        try { 
            return  Integer.parseInt(value);
        } catch(NumberFormatException e) {
            throw new IOException("Config value: " + key + "=" + value + " is not an integer in file: " + m_filePath); 
        } 

    }

    /**
     * <p>Performs lookup of an float config value</p>
     * @throws NumberFormatException thrown if the value cannot be parsed as a float
     * @param key The unique configuration element key
     * @param dflt The default value to be returned if no element with <i>key</i> is found
     * @return The value stored in the configuration identified by the <i>key</i> represented as a float
     */
    public float getFloat(String key, float dflt) throws NumberFormatException {
        String value = getProperty(key);
        if(null == value) {
            return dflt;
        }
        return Float.parseFloat(value);
    }

    /**
     * <p>Performs lookup of a float config value.  Throws an IOException if the key is not found in the configuration</p>
     * @throws IOException thrown if the key is not found in the configuration
     * @throws NumberFormatException thrown if the value cannot be parsed as a float
     * @param key The unique configuration element key
     * @return The value stored in the configuration identified by the <i>key</i> represented as a float
     */
    public float getFloat(String key) throws IOException, NumberFormatException {
        String value = getProperty(key);
        if(null == value) {
            errorMessage(key);
        }
        try {
            return Float.parseFloat(value);
        } catch(NumberFormatException e) {
            throw new IOException("Config value: " + key + "=" + value + " is not a float in file: " + m_filePath); 
        }
    }

    /**
     * <p>Performs lookup of an boolean config value</p>
     * @param key The unique configuration element key
     * @param dflt The default value to be returned if no element with <i>key</i> is found
     * @return The value stored in the configuration identified by the <i>key</i> represented as a boolean.  A boolean configuration value should be expressed as either <i>true</i> or <i>false</i>.  
     */
    public boolean getBoolean(String key, boolean dflt) {
        String value = getProperty(key);
        if(null == value) {
            return dflt;
        }
        return toBoolean(value);
    }

   /**
     * <p>Performs lookup of a boolean config value.  Throws an IOException if the key is not found in the configuration</p>
     * @throws IOException thrown if the key is not found in the configuration
     * @param key The unique configuration element key
     * @return The value stored in the configuration identified by the <i>key</i> represented as a boolean.  A boolean configuration value should be expressed as either <i>true</i> or <i>false</i>.  
     */
    public boolean getBoolean(String key) throws IOException {
        String value = getProperty(key);
        if(null == value) {
            errorMessage(key);
        } 
        return toBoolean(value);
    }


    /**
     * A utility method to lookup an environment variable
     * @param name A system environment variable name
     * @return The system environment variable value
     */
    public static String getEnvironmentVariable(String name) {
        String value = null;
        if(null != name) {
            value = System.getenv().get(name);
        }
        return value;
    }

    /**
     * A utility method to remove quotations around a string 
     * @param value A string possibly enclosed in quotations
     * @return The string with the quotations removed
     */
    public static String unQuote(String value) {
        if(null != value && value.length() > 1) {
            if('"' == value.charAt(value.length() -1) && '"' == value.charAt(0)) {
                value = value.substring(1, value.length() - 1);
            }

        }
        return value;
    }

    /**
     * Reinterprets the input value to the value of the environment variable if the value is in the format
     * of an environment variable
     */
    private String lookupEnvVar(String value) {

        if(null != value && value.length() > 0 && '$' == value.charAt(0)) {
            StringBuilder sb = new StringBuilder(value);
            for(int index = 0; index < sb.length(); index++) {
                //
                // Strip out any environment variable formatting tags
                //
                switch(sb.charAt(index)) {
                    case '$':
                    case '(':
                    case '{':
                    case ')':
                    case '}':
                        sb.setCharAt(index, ' ');
                        break;
                }
            }
            //m_log.debug("ENV LOOKUP:"+sb.toString().trim() + ":" +  getEnvironmentVariable(sb.toString().trim()));
            return getEnvironmentVariable(sb.toString().trim());
        }
        return value;
    }


    private boolean toBoolean(String value) {
        if(null != value && "true".equalsIgnoreCase(value)) {
            return true;
        }
        return false; 
    }

    private void errorMessage(String key) throws IOException {
        throw new IOException("Config value: " + key + " is empty or missing in file: " + m_filePath); 
    }


/*
    public static void main(String [] args) {
        try {
            Configuration c = new Configuration("../../conf/sip-conference.conf");
            System.out.println("RC:'"+ c.getString("mediaprox.home.dir") + "'");
        } catch(Exception e) {
            System.out.println(e);
        }
    }
*/

}
