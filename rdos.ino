/*
  RDOS - "Hard drive" for Tandy 100/102
 */
#include <SD.h>
#include <SoftwareSerial.h>

// Hard drive activity light on pin 7
#define HDD_IND 7
#define BOOT_BUTTON A4

#define NO_FILE 75535

// Set the serial port to be on 62,63 (A8,A9 pins)
SoftwareSerial mySerial(62, 63); // RX, TX

// Display debug output
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINT1(x,y) Serial.print(x,y)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTLN1(x,y) Serial.println(x,y)

// Do not display debug output
//#define DEBUG_PRINT(x) 
//#define DEBUG_PRINT1(x,y)
//#define DEBUG_PRINTLN(x) 
//#define DEBUG_PRINTLN1(x,y) 

char preamble[2];
unsigned char data[255];
int bufpos;
int command_type;
int length;
int checksum;
char cwd[255] = "/";

char pathname[255];
char filename[25];
File selected_file;
int selected_file_open;
int selected_file_mode;
unsigned long selected_file_size;
unsigned long timeout;

int state;
#define initial_state 0
#define start_preamble 1
#define got_preamble 2
#define got_command_type 3
#define getting_data 4
#define processing_command 5

void setup() {
  // Open serial communications for debugging
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  // Set the hard drive light pin
  pinMode(HDD_IND,OUTPUT);
  
  // Set up boot button
  pinMode(BOOT_BUTTON,INPUT_PULLUP);
  
  // Note that even if it's not used as the CS pin, the hardware SS pin 
  // (53 on the Mega) must be left as an output or the SD library 
  // functions will not work. 
  // Note that we connect to this pin through the SPI header.  The SD
  // shield doesn't stretch to pin 53 on the Mega.
  pinMode(53, OUTPUT);
  
  Serial.print("Initializing SD card...");
  if (!SD.begin(8))
  {
    Serial.println("Card failed, or not present");
  } 
  else {
    Serial.println("card initialized.");
  }
  
  // set the data rate for the SoftwareSerial port
  mySerial.begin(19200);
  bufpos = 0;
  timeout = 0;
  
  selected_file_open = 0;
  selected_file_mode = 0;

  // Clear the trash out  
  while(mySerial.available()) {
      mySerial.read();
  }
  
  state = initial_state;
}

void loop() {
  
  // If there's data available
  if (mySerial.available()) {
    timeout = 0;
  
    switch(state) {
      case initial_state: // We started getting data
        preamble[0] = mySerial.read();
        if (preamble[0] == 'Z') {
          DEBUG_PRINTLN("start preamble");
          state = start_preamble;
          digitalWrite(HDD_IND,HIGH);  // turn the drive indicator on
        }
        else {
          state=initial_state;  // Ignore the input
          digitalWrite(HDD_IND,LOW);  // turn the drive indicator off        
          DEBUG_PRINT("BAD preamble 0:");
          debugChar(preamble[0]);
          DEBUG_PRINTLN("");
          preamble[0]=0;
        }
        break;
        
      case start_preamble: // second char read
        preamble[1] = mySerial.read();
        if (preamble[1] == 'Z') {
          DEBUG_PRINT("got preamble:");
          DEBUG_PRINT(preamble[0]);
          DEBUG_PRINTLN(preamble[1]);
          state = got_preamble; // we have the pre-amble
        }
        else {
          state=initial_state;  // Ignore the input
          digitalWrite(HDD_IND,LOW);  // turn the drive indicator off        
          DEBUG_PRINT("BAD preamble 1:");
          debugChar(preamble[1]);
          DEBUG_PRINTLN("");
          preamble[1]=0;
        }
        break;
      
      case got_preamble:
        command_type = mySerial.read();
        state=got_command_type;
        DEBUG_PRINT("got command type:");
        DEBUG_PRINTLN1(command_type,HEX);
        break;
        
      case got_command_type: // we got the command type
        length = mySerial.read(); // read the data length
        if (length > 0) {
          state = getting_data;
        } else {
          state = processing_command; // indicate that we got all the information we need to process the command
        }
        DEBUG_PRINT("got length:");
        DEBUG_PRINTLN1(length,HEX);
        bufpos=0;
        break;
      
      case getting_data: // we have a length
        // Get the next char and put it away
        data[bufpos++] = mySerial.read();
        DEBUG_PRINT("Got byte ");
        DEBUG_PRINT(bufpos);
        DEBUG_PRINT(" of ");
        DEBUG_PRINT(length);
        DEBUG_PRINT(":");
        debugChar(data[bufpos-1]);
        DEBUG_PRINTLN(" ");

        if (bufpos >= length) {
          // yes
          state = processing_command; // indicate that we got all the information we need to process the command
        }
        break;
                        
    } // switch
  } else {
    // We haven't seen data in a long time, timeout.
    if (state != initial_state) {
      timeout++;
      if (timeout > 1000) {
        DEBUG_PRINTLN("Timeout!");
        timeout =0;
        state=initial_state;
        digitalWrite(HDD_IND,LOW);  // turn the drive indicator off
      }
    }

    // If we aren't receiving data
    if (state == initial_state) {
      // Check for the boot button
      if (digitalRead(BOOT_BUTTON) != HIGH) {
        digitalWrite(HDD_IND,HIGH);
        boot_sequence();
        digitalWrite(HDD_IND,LOW);
        state == initial_state;
      }
    }
  }
  
  // We have all the data for processing the command
  if (state == processing_command) {
    timeout = 0;

    DEBUG_PRINT("Processing command type = ");
    DEBUG_PRINT1(command_type,HEX);
    DEBUG_PRINT(" length = ");
    DEBUG_PRINTLN1(length,HEX);

    // Command type > 0x40 means that the client wants to access the second bank on the TPDD2.
    // We don't have banks, so just strip it off.
    if (command_type > 0x40) command_type = command_type & 0xBF;

    switch(command_type) {
      case 0x00:  /* Directory ref */
        process_directory_command();
        break;
      case 0x01:  /* Open file */
        open_file(data[0]);
        break;
      case 0x02:  /* Close file */
        close_file();
        break;
      case 0x03:  /* Read */
        read_file();
        break;
      case 0x04:  /* Write */
        write_file();
        break;
      case 0x05:  /* Delete */
        delete_file();
        break;
      case 0x06: /* Seek */
        seek_file();
        break;
      case 0x07: /* Read a record from file */
        read_record_file();
        break;
      case 0x0D:  /* Rename File */
        rename_file();
        break;
    default:
        Serial.print("Unknown command type:");
        Serial.println(command_type,HEX);
        break;
    } // Command type switch

    state=initial_state;
    digitalWrite(HDD_IND,LOW);

  }
}

void process_directory_command() {
  DEBUG_PRINT("Processing directory ref: ");

  int search_form = data[length-1];
  DEBUG_PRINTLN1(search_form,HEX);
  
  switch (search_form)
  {
    case 0x00:  /* Pick file for open/delete */
      pick_file();
      break;

    case 0x01:  /* "first" directory block */
        get_first_directory_entry();
        break;

    case 0x02:  /* "next" directory block */
        get_next_directory_entry();
        break;

    case 0x03: /* Change directory */
        change_directory();
        break;
        
    default:
        Serial.print("Unknown directory command:");
        Serial.println(search_form,HEX);
        break;
    }
}

void pick_file() {
  DEBUG_PRINTLN("Pick file for open/delete");
    
  // Get the file from the input
  memcpy(filename,data,24);
  filename[24]=0;
  
  // Remove trailing spaces
  for(int i=23; i > 0 && filename[i] == ' '; i--) {
    filename[i] = 0;
  }

  strcpy(pathname,cwd);
  strcat(pathname,filename);
  DEBUG_PRINT("Picked file:");
  DEBUG_PRINTLN(pathname);
  
  // Open file
  selected_file = SD.open(pathname);
  
  // If the file doesn't exist
  if (! selected_file) {
    selected_file_size = NO_FILE;  // Indicate that we didn't find the file
    selected_file_open = 0;
    directory_ref_return(NULL,0,false);
  }
  else { // the file exists
    selected_file_size = selected_file.size();
    selected_file_open = 1;
    directory_ref_return(filename,selected_file_size,selected_file.isDirectory());
  }
}

void get_first_directory_entry() {
  DEBUG_PRINTLN("Get first directory block");

  // If the directory is still open, close it
  if (selected_file_open) {
    selected_file.close();
    selected_file_open = 0;
  }
  
  // Open the root directory of the card
  selected_file = SD.open(cwd);
  selected_file_open = 1;
  selected_file.rewindDirectory();
  
  // Get the first file
  File entry =  selected_file.openNextFile();
  
  // If there was no file, return a no file
  if (! entry) {
    // blank file name is "no files"
    directory_ref_return (NULL, 0, false);
    DEBUG_PRINTLN("No first file");
  }
  else { // there was a file, return the information about the file
    directory_ref_return (entry.name(), entry.size(), entry.isDirectory());
    strcpy(filename,entry.name()); // Remember for get_previous call
    entry.close();
    DEBUG_PRINT("First file:");
    DEBUG_PRINTLN(filename);
  }

}

void change_directory() {
  DEBUG_PRINTLN("Change directory");

  // Get the file from the input
  memcpy(filename,data,24);
  filename[24]=0;
  
  // Remove trailing spaces
  for(int i=23; i > 0 && filename[i] == ' '; i--) {
    filename[i] = 0;
  }

  // If the directory is still open, close it
  if (selected_file_open) {
    selected_file.close();
    selected_file_open = 0;
  }

  // Are we going up one dir?
  if (filename[0] == '.' && filename[1] == '.') {
    // Are we below the root directory?
    if (strlen(cwd) > 1) {
      cwd[strlen(cwd)-1] = 0;
      // Zero out string until we find the last "/"
      for(int i = strlen(cwd); i >0 && cwd[i] != '/'; i--) {
        cwd[i] = 0;
      }
    }
    
    if (strlen(cwd) < 1) {
      cwd[0] = '/';
      cwd[1] = 0;
    }
  } else {
    // If the resulting directory is too long, error
    if (strlen(cwd) + strlen(filename) +1 > 255) {
      normal_return (0x31);
    }
    // Add the new directory
    strcat(cwd,filename);
    strcat(cwd,"/");
  }
  
  DEBUG_PRINT("New directory:");
  DEBUG_PRINTLN(cwd);
  
  // Open the directory
  selected_file = SD.open(cwd);

  // If the new "directory" is not a directory, error
  if (! selected_file.isDirectory()) {
    cwd[strlen(cwd)-1] = 0;
    // Zero out string until we find the last "/"
    for(int i = strlen(cwd); i >0 && cwd[i] != '/'; i--) {
      cwd[i] = 0;
    }
    normal_return (0x31);
  }
  selected_file.close();  
  selected_file_open = 0;

  normal_return (0x00);
}

void get_next_directory_entry() {
  DEBUG_PRINTLN("Get next directory block");

  // If we never did a get_first, we shouldn't be able to do a get next
  if (! selected_file_open) {
    directory_ref_return (NULL, 0,false); // return no file
    Serial.println("Next dir: dir not open");
    return;
  }

  // Get the next file in the directory
  File entry =  selected_file.openNextFile();
  
  // If there wasn't one, return no files
  if (! entry) {
    // blank file name is "no files"
    directory_ref_return (NULL, 0,false);
    DEBUG_PRINTLN("Next dir: no more files");
  }
  else { // there was a file, return the information about the file
    directory_ref_return (entry.name(), entry.size(), entry.isDirectory());
    strcpy(filename,entry.name()); // Remember for get_previous call
    entry.close();
    DEBUG_PRINT("Next file:");
    DEBUG_PRINTLN(filename);
  }
}

void directory_ref_return(char *file_name, int file_size, bool isDirectory)
{
  unsigned short size;
  unsigned char *dotp;
  unsigned char *p;
  unsigned i;
  unsigned char temp;

  memset(data,0,31);

  // If we have a file name to process
  if (file_name)
  {
    // Blank out the file name
    memset (data, ' ', 24);

    // Copy the file name to the buffer
    for (i = 0; i < min (strlen ((char *)file_name), 24); i++)
    {
      data[i] = file_name[i];
    }

    // Attribute = 'F'
    if (isDirectory) {
      data[24] = 'D';
    } else {
      data[24] = 'F';
    }

    size = file_size;

    // Put the file size in the buffer
    memcpy (data + 25, &size, 2);
    // We need to swap the bytes in the size
    temp=data[25];
    data[25]=data[26];
    data[26]=temp;
  }

  data[27]=0x28; // 40 sectors free

  // Send the data back
  command_type = 0x11;
  length = 0x1C;
  send_data(command_type,data,28);
}

void open_file(int omode)
{
  DEBUG_PRINT("Processing open file:");
  DEBUG_PRINTLN(filename);
  
  // If we have a file open, close it
  if (selected_file_open) {
    selected_file.close();
    selected_file_open = 0;
  }
  
  strcpy(pathname,cwd);
  strcat(pathname,filename);

  switch(omode) {
    case 0x01:  /* New file for my_write */
      DEBUG_PRINTLN("New file for write");
      
      // if the file exists, remove it
      if (SD.exists(pathname)) {
        DEBUG_PRINTLN("Removing old file");
        SD.remove(pathname);
      }
      // Fall through
      
    case 0x02:  /* existing file for append */
      DEBUG_PRINTLN("Opening file for append");
      selected_file = SD.open(pathname,FILE_WRITE);
      selected_file_open=1;
      break;
      
    case 0x03:  /* Existing file for read */
      DEBUG_PRINTLN("Existing for read");
      selected_file = SD.open(pathname,FILE_READ);
      selected_file_open=1;
      break;
    }
    
    // If the file didn't open
    if (!selected_file) {
      selected_file_open =0;
      normal_return (0x37); // Error opening file
    }
    else {
      // If the file is too large
      if (selected_file.size() > 65535) {
        selected_file_open =0;
        normal_return (0x6E);// file too long error
      }
      else { // everything's good
        selected_file_open =1;
        normal_return (0x00);
        selected_file_mode = omode;
      }    
    }
}

void close_file() {
  DEBUG_PRINTLN("Processing close file");

  if (selected_file_open) {
    selected_file.close();
    selected_file_open = 0;
  }
  
  normal_return(0x00);
}

void read_file() {
  DEBUG_PRINTLN("Processing read file");
  
  // return type 10
  command_type=0x10;

  // If the file is not open
  if(! selected_file_open)
  {
    normal_return(0x30); // no file name error
    return;
  }

  // If we didn't open the file for read
  if(selected_file_mode!=3)
  {
    normal_return(0x37);  // open format mismatch
    return;
  }

  // get the next 128 bytes from the file
  length = selected_file.read(data, 128);

  // Calculate the check sum of the message
  send_data(command_type, data, length);
}

void write_file() {
  DEBUG_PRINTLN("Processing write file");

  // if the file is not open
  if (!selected_file_open) {
    normal_return(0x30); // no file name
    return;
  }
			
  // If the open mode is not write
  if(selected_file_mode!=1 && selected_file_mode !=2) {
    normal_return(0x37); // open format mismatch
    return;
  }
			
  // Write the data out to the file
  if(selected_file.write(data,length)!=length) {
    normal_return(0x4a); // sector number error
  }
  else
  {
    normal_return(0x00); // everything's OK
  }
			
}

void delete_file() {
  DEBUG_PRINTLN("Processing delete file");
  
  // If the file is open, remove it
  if (selected_file_open) selected_file.close();

  // Remove the file
  SD.remove(filename);
  
  normal_return(0x00);
}

void seek_file() {
  DEBUG_PRINTLN("Processing seek file");
  unsigned long pos;
  
  // if the file is not open
  if (!selected_file_open) {
    normal_return(0x30); // no file name
    return;
  }

  pos = data[0] + data[1]*256;
  DEBUG_PRINT("Position:");
  DEBUG_PRINTLN(pos);
  
  if (selected_file.seek(pos)) {
    normal_return(0); // worked
  }
  else {
    normal_return(0x4a); // Sector number error
  }
  
}

void read_record_file() {
  int len;

  DEBUG_PRINTLN("Processing read record file");
  
  len = data[0];
  DEBUG_PRINT("Record length:");
  DEBUG_PRINTLN(len);

  // return type 10
  command_type=0x10;

  // If the file is not open
  if(! selected_file_open)
  {
    normal_return(0x30); // no file name error
    return;
  }

  // If we didn't open the file for read
  if(selected_file_mode!=3)
  {
    normal_return(0x37);  // open format mismatch
    return;
  }

  // get the next len bytes from the file
  length = selected_file.read(data, len);

  // Calculate the check sum of the message
  send_data(command_type, data, length);
}

void rename_file() {
  DEBUG_PRINTLN("Processing file rename command");
  char newfile_name[24];
  int in;
  
  // Get the new file name from the input
  memcpy(newfile_name,data,24);
  newfile_name[24]=0;
  
  // Remove trailing spaces
  for(int i=23; i > 0 && newfile_name[i] == ' '; i--) {
    newfile_name[i] = 0;
  }

  // You can't rename a file to something that's already there
  if (SD.exists(newfile_name)) {
    normal_return(0x4A); // sector number error
    return;
  }
  DEBUG_PRINTLN("New file does not exist");

  // There's no SD method for renaming a file.
  // So we open the new file, copy the data from the old file, then delete the old file
  
  File oldfile = SD.open(filename,FILE_READ);
  DEBUG_PRINTLN("Open old file");
  File newfile = SD.open(newfile_name,FILE_WRITE);
  DEBUG_PRINTLN("Open new file");

  in = oldfile.read(data,200);
  while (in == 200) {
    DEBUG_PRINTLN("Copying old to new");
    newfile.write(data,200);
    in = oldfile.read(data,200);
  }
  if (in > 0) {
    newfile.write(data,in);
  }

  newfile.close();
  oldfile.close();
  
  DEBUG_PRINTLN("Removing old");
  SD.remove(filename);
  normal_return(0x00);
}

void boot_sequence() {
  int in;
  
  DEBUG_PRINTLN("Booting...");
  
  File loader = SD.open("/RLOAD.BA",FILE_READ);
  DEBUG_PRINTLN("Opened RLOAD.BA");
  
  // Read a block of data
  in = loader.read(data,200);
  while (in == 200) {
    // Send it
    
    alt_send_data(data,200);
    // Get another block
    in = loader.read(data,200);
  }
  // If we have some extra data to send, send it
  if (in > 0) {
    alt_send_data(data,in);
  }

  // Tell the LOAD we are done
  data[0] = 26; // control-Z
  alt_send_data(data,1);
  
  loader.close();
  DEBUG_PRINTLN("Boot complete");
}

void debugChar(unsigned char data_char) {
  DEBUG_PRINT("0x");
  DEBUG_PRINT1(data_char,HEX);
  
  DEBUG_PRINT("(");
  if (data_char > 31 && data_char < 127) {
    DEBUG_PRINT((char)data_char);
  }
  DEBUG_PRINT(") ");  
}

void send_data(unsigned char return_type, unsigned char data[], int length) {
  int x;
  
  // We are ready to send
  DEBUG_PRINT("0x");
  mySerial.write(return_type);
  DEBUG_PRINT1(return_type,HEX);
  DEBUG_PRINT("() ");

  DEBUG_PRINT("0x");
  mySerial.write(length);
  DEBUG_PRINT1(length,HEX);
  DEBUG_PRINT("() ");
  
  x =2;
  for(int i=0; i < length; i++) {
    mySerial.write(data[i]);
    DEBUG_PRINT("0x");
    DEBUG_PRINT1(data[i],HEX);
    
    DEBUG_PRINT("(");
    if (data[i] > 31 && data[i] < 127) {
      DEBUG_PRINT((char)data[i]);
    }
    DEBUG_PRINT(") ");
    x++;
    if (x > 9) {
      DEBUG_PRINTLN(" ");
      x = 0;
    }
  }
}

#define DELAY_BETWEEN_LINES 500
void alt_send_data(unsigned char data[], int length) {
  int x;
  x = 0;
  for(int i=0; i < length; i++) {
    mySerial.write(data[i]);
    DEBUG_PRINT("0x");
    DEBUG_PRINT1(data[i],HEX);
    
    DEBUG_PRINT("(");
    if (data[i] > 31 && data[i] < 127) {
      DEBUG_PRINT((char)data[i]);
    }
    DEBUG_PRINT(") ");
    x++;
    if (x > 9) {
      DEBUG_PRINTLN(" ");
      x = 0;
    }

    if (data[i] == 13) {
      delay(DELAY_BETWEEN_LINES);
    }
  }
}

void normal_return(unsigned char type)
{
  command_type = 0x12;
  length = 0x01;
  data[0] = type;
  
  send_data(command_type, data, length);
}

void dump_data() {
  for(int i = 0; i < bufpos; i++) {
    DEBUG_PRINT1(data[i],HEX);
    DEBUG_PRINT(",");
  }
  DEBUG_PRINTLN("");
}

