#include "SDInterface.h"
#include "lang_var.h"


bool SDInterface::initSD() {
  #if defined HAS_SD && !defined USE_SD_MMC_INTERFACE
    String display_string = "";

    #ifdef KIT
      pinMode(SD_DET, INPUT);
      if (digitalRead(SD_DET) == LOW) {
        esp32m_println(F("SD Card Detect Pin Detected"));
      }
      else {
        esp32m_println(F("SD Card Detect Pin Not Detected"));
        this->supported = false;
        return false;
      }
    #endif

    pinMode(SD_CS, OUTPUT);

    delay(10);
    #if defined(MARAUDER_M5STICKC)
      /* Set up SPI SD Card using external pin header
      StickCPlus Header - SPI SD Card Reader
                  3v3   -   3v3
                  GND   -   GND
                   G0   -   CLK
              G36/G25   -   MISO
                  G26   -   MOSI
                        -   CS (jumper to SD Card GND Pin)
      */
      enum { SPI_SCK = 0, SPI_MISO = 36, SPI_MOSI = 26 };
      this->spiExt = new SPIClass();
      this->spiExt->begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
      if (!SD.begin(SD_CS, *(this->spiExt))) {
    #else
      if (!SD.begin(SD_CS)) {
    #endif
      esp32m_println(F("Failed to mount SD Card"));
      this->supported = false;
      return false;
    }
    else {
      this->supported = true;
      this->cardType = SD.cardType();
      //if (cardType == CARD_MMC)
      //  esp32m_println(F("SD: MMC Mounted"));
      //else if(cardType == CARD_SD)
      //    esp32m_println(F("SD: SDSC Mounted"));
      //else if(cardType == CARD_SDHC)
      //    esp32m_println(F("SD: SDHC Mounted"));
      //else
      //    esp32m_println(F("SD: UNKNOWN Card Mounted"));

      this->cardSizeMB = SD.cardSize() / (1024 * 1024);
    
      //Serial.printf("SD Card Size: %lluMB\n", this->cardSizeMB);

      if (this->supported) {
        const int NUM_DIGITS = log10(this->cardSizeMB) + 1;

        char sz[NUM_DIGITS + 1];

        sz[NUM_DIGITS] =  0;
        for ( size_t i = NUM_DIGITS; i--; this->cardSizeMB /= 10)
        {
            sz[i] = '0' + (this->cardSizeMB % 10);
            display_string.concat((String)sz[i]);
        }
  
        this->card_sz = sz;
      }

      if (!SD.exists("/SCRIPTS")) {
        esp32m_println("/SCRIPTS does not exist. Creating...");

        SD.mkdir("/SCRIPTS");
        esp32m_println("/SCRIPTS created");
      }

      this->sd_files = new LinkedList<String>();

      this->sd_files->add("Back");
    
      return true;
  }
  #else
    //esp32m_println("SD support disabled, skipping init");
    return false;
  #endif
}

File SDInterface::getFile(String path) {
  if (this->supported) {
    File file = SD.open(path, FILE_READ);

    //if (file)
    return file;
  }
}

bool SDInterface::removeFile(String file_path) {
  if (SD.remove(file_path))
    return true;
  else
    return false;
}

void SDInterface::listDirToLinkedList(LinkedList<String>* file_names, String str_dir, String ext) {
  if (this->supported) {
    File dir = SD.open(str_dir);
    while (true)
    {
      File entry = dir.openNextFile();
      if (!entry)
      {
        break;
      }

      if (entry.isDirectory())
        continue;

      String file_name = entry.name();
      if (ext != "") {
        if (file_name.endsWith(ext)) {
          file_names->add(file_name);
        }
      }
      else
        file_names->add(file_name);
    }
  }
}

void SDInterface::listDir(String str_dir){
  if (this->supported) {
    File dir = SD.open(str_dir);
    while (true)
    {
      File entry = dir.openNextFile();
      if (! entry)
      {
        break;
      }
      //for (uint8_t i = 0; i < numTabs; i++)
      //{
      //  esp32m_print('\t');
      //}
      esp32m_print(entry.name());
      esp32m_print("\t");
      esp32m_println(entry.size());
      entry.close();
    }
  }
}

void SDInterface::runUpdate() {
  #ifdef HAS_SCREEN
    display_obj.tft.setTextWrap(false);
    display_obj.tft.setFreeFont(NULL);
    display_obj.tft.setCursor(0, TFT_HEIGHT / 3);
    display_obj.tft.setTextSize(1);
    display_obj.tft.setTextColor(TFT_WHITE);
  
    display_obj.tft.println(F(text15));
  #endif
  File updateBin = SD.open("/update.bin");
  if (updateBin) {
    if(updateBin.isDirectory()){
      #ifdef HAS_SCREEN
        display_obj.tft.setTextColor(TFT_RED);
        display_obj.tft.println(F(text_table2[0]));
      #endif
      esp32m_println(F("Error, could not find \"update.bin\""));
      #ifdef HAS_SCREEN
        display_obj.tft.setTextColor(TFT_WHITE);
      #endif
      updateBin.close();
      return;
    }

    size_t updateSize = updateBin.size();

    if (updateSize > 0) {
      #ifdef HAS_SCREEN
        display_obj.tft.println(F(text_table2[1]));
      #endif
      esp32m_println(F("Starting update over SD. Please wait..."));
      this->performUpdate(updateBin, updateSize);
    }
    else {
      #ifdef HAS_SCREEN
        display_obj.tft.setTextColor(TFT_RED);
        display_obj.tft.println(F(text_table2[2]));
      #endif
      esp32m_println(F("Error, file is empty"));
      #ifdef HAS_SCREEN
        display_obj.tft.setTextColor(TFT_WHITE);
      #endif
      return;
    }

    updateBin.close();
    
      // whe finished remove the binary from sd card to indicate end of the process
    #ifdef HAS_SCREEN
      display_obj.tft.println(F(text_table2[3]));
    #endif
    esp32m_println(F("rebooting..."));
    //SD.remove("/update.bin");      
    delay(1000);
    ESP.restart();
  }
  else {
    #ifdef HAS_SCREEN
      display_obj.tft.setTextColor(TFT_RED);
      display_obj.tft.println(F(text_table2[4]));
    #endif
    esp32m_println(F("Could not load update.bin from sd root"));
    #ifdef HAS_SCREEN
      display_obj.tft.setTextColor(TFT_WHITE);
    #endif
  }
}

void SDInterface::performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {   
    #ifdef HAS_SCREEN
      display_obj.tft.println(text_table2[5] + String(updateSize));
      display_obj.tft.println(F(text_table2[6]));
    #endif
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      #ifdef HAS_SCREEN
        display_obj.tft.println(text_table2[7] + String(written) + text_table2[10]);
      #endif
      esp32m_println("Written : " + String(written) + " successfully");
    }
    else {
      #ifdef HAS_SCREEN
        display_obj.tft.println(text_table2[8] + String(written) + "/" + String(updateSize) + text_table2[9]);
      #endif
      esp32m_println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
    }
    if (Update.end()) {
      esp32m_println("OTA done!");
      if (Update.isFinished()) {
        #ifdef HAS_SCREEN
          display_obj.tft.println(F(text_table2[11]));
        #endif
        esp32m_println(F("Update successfully completed. Rebooting."));
      }
      else {
        #ifdef HAS_SCREEN
          display_obj.tft.setTextColor(TFT_RED);
          display_obj.tft.println(text_table2[12]);
        #endif
        esp32m_println("Update not finished? Something went wrong!");
        #ifdef HAS_SCREEN
          display_obj.tft.setTextColor(TFT_WHITE);
        #endif
      }
    }
    else {
      #ifdef HAS_SCREEN
        display_obj.tft.println(text_table2[13] + String(Update.getError()));
      #endif
      esp32m_println("Error Occurred. Error #: " + String(Update.getError()));
    }

  }
  else
  {
    #ifdef HAS_SCREEN
      display_obj.tft.println(text_table2[14]);
    #endif
    esp32m_println("Not enough space to begin OTA");
  }
}

bool SDInterface::checkDetectPin() {
  #ifdef KIT
    if (digitalRead(SD_DET) == LOW)
      return true;
    else
      return false;
  #endif

  return false;
}

void SDInterface::main() {
  if (!this->supported) {
    if (checkDetectPin()) {
      delay(100);
      this->initSD();
    }
  }
}
