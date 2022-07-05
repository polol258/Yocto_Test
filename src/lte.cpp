#define LOG_TAG "PT_LTE"

#include <stdlib.h>
#include <stdio.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <fcntl.h>
#include <ctype.h>
#include <fstream>
#include <memory>
#include <iostream>
#include <sstream>


#include "shell.h"
#include "atmodem.h"


#define LTE_NODE   "/dev/ttyACM2"
#define UART_NODE  "/dev/ttyACM1"

#define MINI_FUNC_MODE   0
#define NORMAL_FUNC_MODE 1
#define RESET_FUNC_MODE  2

#define MASTER_ANTENNA 0
#define SLAVE_ANTENNA  1
#define DUAL_ANTENNA   2

#define SERIAL_PORT    0
#define USB_PORT       1

#define LTE_ECM        0
#define LTE_WWAN       1


using namespace std;
using namespace pt_tool;

static uint8_t port_select = 1; /* 1 : USB Port, 0 : Serial Port*/

static std::vector<std::string> splitString(const std::string& str)
{
    std::vector<std::string> tokens;
 
    std::stringstream str_buf(str);
    std::string token;


    while (std::getline(str_buf, token, '\n')) {
        tokens.push_back(token);
    }
 
    return tokens;
}

static bool writeCommand(std::string command, std::string keyword,std::string &response,int response_num) {

    pt_tool::AtModem at;

    bool result = false;
    bool read_finish = false;

    std::string path = (port_select == SERIAL_PORT) ? UART_NODE : LTE_NODE;

    std::string cmd = command + "\r";
    std::string cmp_str;
 
    int retry_cnt = 0;
    int read_cnt = 0;

    do {
        
        syslog(LOG_INFO,"[%s] : write port : %s.",__FUNCTION__, path.c_str());

        if (!at.open(path)) {
            syslog(LOG_ERR,"[%s] : Port open fail.", __FUNCTION__);
            return false;  
        }

        if (!at.write(cmd)) {
            syslog(LOG_ERR,"[%s] : Write error.", __FUNCTION__);
            at.close();
            return false;  
        }
        
        syslog(LOG_INFO, "[%s] : %s Command Response :", __FUNCTION__,command.c_str());

        while (at.readLine(10, &cmp_str)) {

            syslog(LOG_INFO,"[%s] : %s", __FUNCTION__,cmp_str.c_str());

            if (keyword.compare("ALL_RESPONSE") == 0) {
                response += cmp_str +"\r\n";
            }
            else if ((keyword.compare("NOP") != 0) && (read_cnt < response_num)) {
                if (cmp_str.find(keyword) != string::npos) {
                    response = cmp_str;
                    read_cnt++;
                }
            }

            if (cmp_str.find("OK") != string::npos) {
                result = true;
                read_finish = true;
                break;
            }

            if ((cmp_str.find("ERROR") != string::npos)) {
                response = ": " + cmp_str;
                result = false;
                read_finish = true;
                break;               
            }
        }

        at.close();

        sleep(1);

    } while ((!read_finish) && (++retry_cnt < 15));

    return result;      
}

static bool Initial_State_Read(void) {

    pt_tool::AtModem at;

    bool result = false;

    std::string cmp_str;
 
    int retry_cnt = 0;

    do {
        
        if (!at.open(std::string(LTE_NODE))) {
            syslog(LOG_ERR, "[%s] : Port open fail.", __FUNCTION__); 
        }
        else {

            while (at.readLine(10, &cmp_str)) {

                syslog(LOG_INFO, "[%s] : %s", __FUNCTION__,cmp_str.c_str());

                if (cmp_str.find("^SYSSTART") != string::npos) {
                    result = true;
                    break;
                }

            }

            at.close();
        }

        sleep(1);

    } while ( (!result) && (++retry_cnt < 20));

    return result;      
}

static bool lte_mode_get(std::string &ret_str) {

    bool result = false;

    std::string cmp_str;

    cmp_str.clear();

    result = writeCommand("AT+CFUN?", "+CFUN:", cmp_str,1);

    if (result) {

        /*Response Format : +CFUN: <fun>*/
        switch(std::atoi(cmp_str.substr(7,1).c_str())) {
            case 0: ret_str = "Minimum Function";
            break;
            case 1: ret_str = "Normal Function";
            break;
            case 4: ret_str = "Airplane";
            break;
            default: ret_str = "UnKnown";
            break;
        }

    }
    else {
        
        if (cmp_str.length() != 0) {
            ret_str = cmp_str;
        }        
    
    }

    return result;   
}

static bool lte_mode_select(uint8_t mode, std::string &ret_str) {

    std::string cmp_str;

    bool result = false;

    cmp_str.clear();

    switch(mode) {
        case MINI_FUNC_MODE:
            result = writeCommand("AT+CFUN=0", "NOP", cmp_str,0);
            break;
        case NORMAL_FUNC_MODE: 
            result = writeCommand("AT+CFUN=1", "NOP", cmp_str,0);
            break;
        case RESET_FUNC_MODE: 
            result = writeCommand("AT+CFUN=1,1", "NOP", cmp_str,0);
            break;
        default: 
            break;
    }

    if (!result) {

        if (cmp_str.length() != 0) {
            ret_str = cmp_str;
        }

    }
    else {

        if (mode == RESET_FUNC_MODE) {
            result = Initial_State_Read(); 
        }

    }

    return result;
}

static bool lteVer(std::string &ret_str) {
    
    bool result = false;
    std::string cmp_str;

    cmp_str.clear();

    result = writeCommand("ATI", "REVISION", cmp_str,1);

    if (result) {
        ret_str = cmp_str.substr(9); //Response Format : REVISION xx.yyy
    }
    else {
    
        if (cmp_str.length() != 0){
            ret_str = cmp_str;
        }
    
    }

    return result;   
}


static bool lteIMEI(std::string &ret_str) {

    bool result = false;

    std::string cmp_str;

    cmp_str.clear();

    result = writeCommand("AT+CGSN", "ALL_RESPONSE", cmp_str,1);

    if (result) {
        ret_str = cmp_str;
    }
    else {
    
        if (cmp_str.length() != 0){
            ret_str = cmp_str;
        }
    
    }

    return result;
}

static bool lteIMSI(std::string &ret_str) {

    bool result = false;

    std::string cmp_str;

    cmp_str.clear();

    result = writeCommand("AT+CIMI", "ALL_RESPONSE", cmp_str,1);

    if (cmp_str.length() != 0) {
        ret_str = cmp_str;
    }

    return result;
}

static bool lteICCID(std::string &ret_str) {

    bool result = false;

    std::string cmp_str;

    cmp_str.clear();

    result = writeCommand("AT+CCID", "+CCID:", cmp_str,1);

    if (result) {
        ret_str = cmp_str.substr(7);
    }
    else {
    
        if (cmp_str.length() != 0){
            ret_str = cmp_str;
        }
    
    }

    return result;
}

static bool lteNUM(std::string &ret_str) {

    bool result = false;

    int first_pos;
    int second_pos;

    std::string cmp_str;

    cmp_str.clear();

    result = writeCommand("AT+CNUM", "+CNUM:", cmp_str,1);

    if (result) {
        first_pos = cmp_str.find_first_of(",");
        second_pos = cmp_str.substr(first_pos+2).find_first_of(",");
        ret_str = cmp_str.substr(first_pos+2,second_pos-1);
    }
    else {
    
        if (cmp_str.length() != 0){
            ret_str = cmp_str;
        }
    
    }

    return result;
}

static bool lte_antenna_sel(const int &index, std::string &ret_str) {

    bool result = false;

    std::string cmp_str;
    std::string sad_setting_str;

    cmp_str.clear();

    result = (index == MASTER_ANTENNA) ? writeCommand("AT^SAD=10", "NOP", cmp_str,0) : \
             (index == SLAVE_ANTENNA)  ? writeCommand("AT^SAD=13", "NOP", cmp_str,0) : \
                                         writeCommand("AT^SAD=11", "NOP", cmp_str,0);
    
    if (!result) {
        goto error_out;
    }

    result = writeCommand("AT&W", "NOP", cmp_str,0);

    if (!result) {
        goto error_out;
    }     

    result = lte_mode_select(RESET_FUNC_MODE,ret_str);

    if (!result) {
        goto error_out;
    }
   
    result = writeCommand("AT^SAD=12", "^SAD:", cmp_str,1);

    if (!result) {
        goto error_out;
    }

    sad_setting_str = (index == MASTER_ANTENNA) ? "10" :
                      (index == SLAVE_ANTENNA) ? "13" :
                                                 "11";

    if (cmp_str.substr(6,2).compare(sad_setting_str) != 0) {
        syslog(LOG_ERR, "[%s] : Setting Error", __FUNCTION__);
        goto error_out;
    }

error_out :
    if (!result) {

        if (cmp_str.length() != 0){
            ret_str = cmp_str; 
        }

    }

    return result;
}

static bool lte_rssi(std::string &ret_str) {
    bool result = false;

    std::string cmp_str;
    
    int first_pos;

    cmp_str.clear();

    result = writeCommand("AT+CSQ", "+CSQ:", cmp_str,1);

    if (result) {
        first_pos = cmp_str.find_first_of(",");
        ret_str = cmp_str.substr(6,first_pos - 6);
    }
    else {

        if (cmp_str.length() != 0){
            ret_str = cmp_str; 
        }

    }

    return result;
}

static bool lte_serial_test(std::string &ret_str) {
    
    bool result = false;

    port_select = SERIAL_PORT;

    result = lteVer(ret_str);

    port_select = USB_PORT;

    return result;
}

static bool lte_usbmode(int mode,std::string &ret_str) {
    bool result = false;

    std::string cmp_str;

    cmp_str.clear();

    switch(mode) {
        case LTE_WWAN:
            result = writeCommand("AT^SSRVSET=\"actSrvSet\",11", "NOP", cmp_str,0);
            break;
        case LTE_ECM: 
            result = writeCommand("AT^SSRVSET=\"actSrvSet\",1", "NOP", cmp_str,0);
            break;
        default: 
            break;
    }

    if (result) {

        if (cmp_str.length() != 0){
            ret_str = cmp_str; 
        }

    }

    return result;
}

int main(int argc, char** argv)
{

    std::string ver_str       = "ver";
    std::string mode_str      = "mode";   
    std::string imsi_str      = "imsi";
    std::string imei_str      = "imei";
    std::string iccid_str     = "iccid";
    std::string num_str       = "num";
    std::string rssi_str      = "rssi";
    std::string serial_str    = "serial";
    std::string usb_str       = "usbmode";  

    std::string ret_str;
    std::string setting;
    
    std::vector<std::string> multi_response;
    
    bool result = false;  

    openlog(argv[0],LOG_PID,LOG_USER);


    if (argc < 2) {
        cout << "Parameter number worng" << endl;
        goto main_out;
    }

    ret_str.clear();

    if (ver_str.compare(argv[1]) == 0) {

        if (lteVer(ret_str)) {
            cout << "Get version = " + ret_str << endl;            
        }
        else {
            cout << "Get version fail " + ret_str << endl; 
        }

    }
    else if (imei_str.compare(argv[1]) == 0) {

        if (lteIMEI(ret_str)) {
            multi_response = splitString(ret_str);
            cout << "Get imei = " + multi_response[1] << endl;            
        }
        else {
            cout << "Get imei fail " + ret_str << endl; 
        }

    }
    else if (imsi_str.compare(argv[1]) == 0) {

        if (lteIMSI(ret_str)) {
            multi_response = splitString(ret_str);
            cout << "Get imsi = " + multi_response[1] << endl;  
        }
        else {

            cout << "Get imsi fail " + ret_str << endl; 
        }

    }
    else if (iccid_str.compare(argv[1]) == 0) {

        if (lteICCID(ret_str)) {
            cout << "Get iccid = " + ret_str << endl;  
        }
        else {                
            cout << "Get iccid fail " + ret_str << endl; 
        }

    }
    else if (num_str.compare(argv[1]) == 0) {
        
        if (lteNUM(ret_str)) {
            cout << "Get number = " + ret_str << endl;  
        }
        else {
            cout << "Get number fail " + ret_str << endl; 
        }

    }
    else if (rssi_str.compare(argv[1]) == 0) {

        if (argc != 3) {
            cout << "Parameter number worng" << endl;
            goto main_out;
        }
        
        std::string setting(argv[2]);

        result = (setting.compare("master") == 0) ? lte_antenna_sel(MASTER_ANTENNA,ret_str): \
                 (setting.compare("slave") == 0) ? lte_antenna_sel(SLAVE_ANTENNA,ret_str): \
                 (setting.compare("dual") == 0) ? lte_antenna_sel(DUAL_ANTENNA,ret_str): false;
        
        if (!result) {
            cout << "antenna setting fail" + ret_str << endl;   
        }
        else {

            /*
                ret_str = 0 => -113 dBm or less
                ret_str = 1 => -111 dBm
                ret_str = 2~30 => -109~-53 dBm
                ret_str = 31 => -51 dBm or greater
                ret_str = 99 => not known or not detectable
            
            */

            ret_str.clear();

            result = lte_rssi(ret_str);

            if (!result) {
                cout << "RSSI get fail" + ret_str << endl;   
            }
            else {
                int rssi_value;

                if(std::atoi(ret_str.c_str()) == 99) {
                    cout << "RSSI value = unknown" << endl; 
                }
                else {
                    rssi_value =  (-113 + std::atoi(ret_str.c_str())*(2));
                    cout << "RSSI value = " + std::to_string(rssi_value) << endl; 
                }
            }


        }
    }
    else if (mode_str.compare(argv[1]) == 0) {

        if ( argc == 2) {

            if (lte_mode_get(ret_str)) {
                cout << "LTE mode = " + ret_str << endl;
            }
            else {
                cout << "LTE mode get fail " + ret_str << endl; 
            }

        }
        else {

            std::string setting(argv[2]);

            if (setting.compare("mini") == 0) {
                result = lte_mode_select(MINI_FUNC_MODE,ret_str);
            } 
            else if (setting.compare("normal") == 0) {
                result = lte_mode_select(NORMAL_FUNC_MODE,ret_str);
            }
            else if (setting.compare("reset") == 0) {
                result = lte_mode_select(RESET_FUNC_MODE,ret_str);
            }
            else {
                cout << "The parameter not support" << endl;
            }

            if(result) {
                cout << "LTE setting finish" << endl;
            }
            else {
                cout << "LTE setting fail " + ret_str << endl; 
            }

        }
    }
    else if (serial_str.compare(argv[1]) == 0) {

        if (lte_serial_test(ret_str)) {
            cout << "Get version from serial port = " + ret_str << endl;            
        }
        else {
            cout << "Failed to get version from serial port " + ret_str << endl; 
        }

    }
    else if (usb_str.compare(argv[1]) == 0) {

        if ( argc < 3) {
            cout << "Parameter number worng" << endl;
        }
        else {

            std::string setting(argv[2]);

            if (setting.compare("ecm") == 0) {
                result = lte_usbmode(LTE_ECM,ret_str);
            } 
            else if (setting.compare("wwan") == 0) {
                result = lte_usbmode(LTE_WWAN,ret_str);
            }
            else {
                cout << "The parameter not support" << endl;
            }

            if(result) {
                cout << "LTE usb setting finish" << endl;
            }
            else {
                cout << "LTE usb setting fail " + ret_str << endl; 
            }
        }
    }  
    else {
        cout << "The parameter not support" << endl;
    }

main_out:

    closelog();

    return 0;

}
