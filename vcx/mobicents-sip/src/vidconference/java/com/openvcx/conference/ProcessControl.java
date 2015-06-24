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

package com.openvcx.conference;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;

import org.apache.log4j.Logger;

/**
 *
 * Provides support for system process start/stop  
 *
 */
public class ProcessControl {

    /**
     * <p>The system shell path</p>
     * <p><i>/bin/bash</i></p>
     */
    public static final String SYSTEM_SHELL             = "/bin/bash";

    /**
     * Starts a system process given a command line array
     * @param arrCommandline An array of command line arguments to be used by the new process.  When starting a script, the first argument should be the path to the shell.
     */
    public static Process startProcess(String [] arrCommandline) throws IOException {
        Process process = null;

        process = Runtime.getRuntime().exec(arrCommandline);

        return process;
    }

    /**
     * <p>Starts a system process given a command line array.</p>
     * <p>This method expects to read the output of the system process.</p>
     * @param arrCommandline An array of command line arguments to be used by the new process.  When starting a script, the first argument should be the path to the shell.
     * @param tmpOutPath A temporary filepath used to read output of the process
     * @param timeoutMs A timeout value in miliseconds to wait for the output of the process.  If the argument is 0 then a timeout of 1000 miliseconds is used.
     * @param processControl An <i>IProcessControl</i> implemented instance used to read the process output
     */
    public static void startProcess(String [] arrCommandline, 
                                    String tmpOutPath, 
                                    long timeoutMs, 
                                    IProcessControl processControl) 
                                        throws Exception {

        File file = new File(tmpOutPath);
        if(file.canRead()) {
            file.delete();
        }

        ProcessControl.startProcess(arrCommandline);

        //
        // Wait until the script creates the temporary output file
        //
        long tm0 = System.currentTimeMillis();
        file = new File(tmpOutPath);

        if(timeoutMs == 0) {
            timeoutMs = 1000;
        }

        while(false == file.canRead() && System.currentTimeMillis() - tm0 < timeoutMs) {
            Thread.sleep(40);
        }

        BufferedReader br = new BufferedReader(new InputStreamReader((new FileInputStream(file)), Charset.forName("UTF-8")));

        processControl.onReadProcessOutput(br);

        if(file.canRead()) {
            file.delete();
        }

    }

    /**
     * Retrieves the process id stored in a PID file
     * @param pidFilePath The filepath of the PID
     * @return The process identifier
     */
    public static int getProcessPid(String pidFilePath) throws IOException {

        BufferedReader br = new BufferedReader(new InputStreamReader((new FileInputStream(
                            new File(pidFilePath))), Charset.forName("UTF-8")));

        String pidString = br.readLine();

        return Integer.parseInt(pidString);
    }

    /**
     * Retrieves the process id stored in a PID file
     * @param pidFilePath The filepath of the file with the PID
     * @param timeoutMs A timeout value in miliseconds to wait for the output of the process.
     * @return The process identifier
     */
    public static int getProcessPid(String pidFilePath, 
                                    long timeoutMs) 
                                    throws IOException, InterruptedException {
        int pid = 0;
        long tmStart = System.currentTimeMillis();

        do {

            try {
                pid = getProcessPid(pidFilePath);
            } catch(FileNotFoundException e) {
                if(System.currentTimeMillis() - tmStart <= timeoutMs) {
                    Thread.sleep(100);
                } else {
                    throw e;
                }
            }

        } while(pid == 0);

        return pid;
    }

}
