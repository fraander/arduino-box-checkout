/**
 * Team 1 - Meal Box Checkout System - v1
 * Frank Anderson
 */

/**
 * Based on code found here for a 4 RFID system: https://github.com/Annaane/MultiRfid
 */

#include <SPI.h> // serial interface library for RFID
#include <MFRC522.h> // RFID library

/**
 * PIN & READER ASSIGNMENTS
 * Reader 0 = check out box
 * Reader 1 = return box
 */

// PIN Numbers : RESET + SDAs
#define RST_PIN         9
#define SS_0_PIN        10
#define SS_1_PIN        8

byte ssPins[] = {SS_0_PIN, SS_1_PIN};

// Create an MFRC522 instance for each reader:
#define NUM_READERS   2
MFRC522 mfrc522[NUM_READERS];

// String to go down RETURN path
#define RETURN_KEY "."

/**
 * Global vars to track program state
 */
String tagID = "";  // id of currently read tag, "" if no tag read yet
String nuid = "";  // "" if waiting for input; "r" if in return process; "#########" if in checkout process
String boxes[10][2]; // store boxes and nuid's (technically can only hold 10 entries, but would use SQL database if actually used in real world)
int numBoxes = 0; // track how many boxes have been added (for efficiency)

/**
 * Set up initial state of the program
 * - Open Serial and RFID interface
 * - Setup each reader
 * - Flush the Serial monitor and print a welcome message
 */
void setup() {
    Serial.begin(9600);           // Initialize serial communications with the PC
    while (!Serial);              // Wait until port is open
    SPI.begin();                  // Init SPI bus

    setupReader(0);               // Setup readers
    setupReader(1);

    flushSerial(-1);              // Clear the Serial monitor of previous run content
    printInstructions();          // Print a welcome message
}

/**
 * Run state of the program
 * Checks if information is in the Serial flow and then passes down either flow path.
 */
void loop() {
    if (Serial.available() > 0) { // is information in the Serial path...
        while (nuid == "" && nuid != "r") { // if not currently on a path... figure out which one to go down
            if (Serial.available() > 0) { // if there's something to read, read it
                readSerial();
            }
        }

        if (nuid == "r") { // return path
            Serial.println("Place your box on the white circle. Wait for the box to scan.");
            while (!getID(1)); // repeat until reading something for checkout

            if (findNUIDfor(tagID) != "-1") {
              Serial.println("Box ID: " + tagID);
              Serial.println("Marked box connected to NUID: " + findNUIDfor(tagID)) + " as returned.";
              Serial.println("Place box back in stack.");
              removeNUIDfor(tagID);
            } else {
              Serial.println("Box not currently checked out.");
              Serial.println("Try returning again if you believe this is an error.");
            }
            
        } else { // check out path
            Serial.println("NUID: " + nuid);
            while (!getID(0)); // repeat until reading something for checkout
            Serial.println("Box ID: " + tagID);
            addBox(tagID, nuid);
            Serial.println("Rotate the wheel 90˚ clockwise and and take your box!");
            Serial.println("Remember to return it within 7 days, or you will be charged.");
        }

        delay(3000); // 1 second delay, then clear instance variables...
        nuid = "";
        tagID = "";
        
        flushSerial(-1); // reset
        printInstructions(); // print instructions for a new user
    }
}

/**
 * Print instructions for user.
 */
void printInstructions() {
  Serial.println("\nType your NUID and press Enter to check out a box.");
  Serial.print("Type ");
  Serial.print(RETURN_KEY);
  Serial.println(" and press Enter to return a box.");  
}

/**
 * Read input from Serial stream and clean input / throw errors
 */
void readSerial() {
    String incomingString = ""; // string to be read
    String cleanedString = ""; // string to be output

    incomingString = Serial.readString(); // read the serial port

    // if return path indicator ...
    if ((incomingString.substring(0, 1) == RETURN_KEY || incomingString.substring(0, 1) == "R") &&
        incomingString.length() < 4) {
        nuid = "r"; // set the current path
        return; // exit the function
    }

    // otherwise ... clean the string for NUID input
    for (int i = 0; i < incomingString.length(); i++) { // only take numbers as valid chars
        if (incomingString.substring(i, i + 1) == "0" ||
            incomingString.substring(i, i + 1) == "1" ||
            incomingString.substring(i, i + 1) == "2" ||
            incomingString.substring(i, i + 1) == "3" ||
            incomingString.substring(i, i + 1) == "4" ||
            incomingString.substring(i, i + 1) == "5" ||
            incomingString.substring(i, i + 1) == "6" ||
            incomingString.substring(i, i + 1) == "7" ||
            incomingString.substring(i, i + 1) == "8" ||
            incomingString.substring(i, i + 1) == "9") { // if valid...
            cleanedString += incomingString.substring(i, i + 1); // add to cleaned string
        }
    }

    if (cleanedString.length() == String("000000000").length()) { // if it's the right length too
        nuid = cleanedString; // set the NUID
    } else { // otherwise, reset and tell them to try again
        cleanedString = ""; // clear instance variables
        incomingString = "";
        Serial.println("Invalid input. Try again.");
        return; // exit  function (this return isn't necessary, but it's good practice in case we add another state below)
    }
}

/**
 * Set up the reader to be readable.
 * Used to take complex line and make it more readable, as well as usable for different readers.
 * @param reader - the reader to setup
 */
void setupReader(int reader) {
    mfrc522[reader].PCD_Init(ssPins[reader], RST_PIN); // init function for mfrc522
}

/**
 * Make a reading if possible, #t if read, #f if nothing to read/could not read
 * @param reader - reader to read from (same process for return/checkout so...yeah)
 * @return - true if read, false if nothing to read/could not read
 */
boolean getID(int reader) {
    if (!mfrc522[reader].PICC_IsNewCardPresent()) { // is there a card there to read?
        return false;
    }

    if (!mfrc522[reader].PICC_ReadCardSerial()) { // can it be read?
        return false;
    }

    tagID = ""; // clear the tag id before it gets reassigned

    for (uint8_t i = 0; i < 4; i++) { // read the tag id
        tagID.concat(String(mfrc522[reader].uid.uidByte[i], HEX));
    }

    tagID.toUpperCase(); // make the tag id uppercase

    mfrc522[reader].PICC_HaltA(); // stop reading

    return true; // Yay! It read something
}

/**
 * Print a bunch of blank lines to the Serial monitor so that the next user doesn't see the previous user's information
 * @param numLines - number of lines to print, if negative prints an amount that totally flushes the monitor.
 */
void flushSerial(int numLines) {
    int linesToPrint = numLines; // how many lines should be printed
    if (numLines < 0) { // if it's a negative number, use a big value
        linesToPrint = 50;
    }

    for (int i = 0; i < linesToPrint; i++) { // print x-many blank lines
        Serial.println("");
    }
}

/**
 * Reassign boxes array with new size for new boxes
 * @param box - id of the box
 * @param nuid - id of user with box
 */
void addBox(String box, String nuid) {
  boxes[numBoxes][0] = nuid; // store nuid
  boxes[numBoxes][1] = box; // and boxid
  numBoxes += 1; // add one to the box count
}

/**
 * Find the box ID in an array of box id's
 * @param boxID - id of box to find
 * @return nuid connected to box, or "-1" if box not found
 */
String findNUIDfor(String box) {
  for (int i = 0; i < numBoxes; i++) { // linear search the array
    if (boxes[i][1] == box) {
      return boxes[i][0]; // return box id if found
    }
  }
  return "-1"; // if not found, -1 is the serial value
}

/**
 * Remove the box ID in an array of box id's by setting the id to "0"
 * @param boxID - id of box to find
 * @return nuid connected to box, or "-1" if box not found
 */
String removeNUIDfor(String box) {
  for (int i = 0; i < numBoxes; i++) {
    if (boxes[i][1] == box) {
      boxes[i][1] = "0"; // "removing is just setting an impossible value"
      return "1"; // 1 == success code
    }
  }
  return "-1"; // -1 == value code
}

/**
 * Prints all boxes in Boxes
 */
void dumpBoxes() {
  for (int i = 0; i < numBoxes; i++) { // for all the boxes in the array...
    Serial.println(boxes[i][0] + "  " + boxes[i][1]); // print nuid and box id
  }
}
