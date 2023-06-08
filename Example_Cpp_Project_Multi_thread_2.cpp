// Alkinoos Sarioglou - Year 3
// ID: 10136315
// Concurrent Systems Assignment
// 18/11/2019


// Pre-processor directives
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <random>
#include <chrono>
#include <vector>
#include <iterator>

using namespace std;

// Global Constants:
int const MAX_NUM_OF_CHAN = 6; //number of AdcInputChannels
int const MAX_NUM_OF_THREADS = 6; //number of threads
int const DATA_BLOCK_SIZE = 20; //size of data array for each thread
int const NUM_OF_LINKS = 3; //number of communication links

int random_delay_between_100_and_500_i = 0; // delay_1 0.1-0.5 s after release of ADC channel
int random_delay_between_100_and_500_ii = 0; // delay_1 0.1-0.5 s after obtaining a Link

// Global Variables:
std::mutex mu;
std::condition_variable cond;
std::condition_variable cond_link;
std::unique_lock<std::mutex> lock1;
std::unique_lock<std::mutex> map_locker;

// Map of threads IDs and int values
std::map<std::thread::id, int> threadIDs;

// Class for Input Channels to ADC
class AdcInputChannel {
    private:
        int currentSample; // current sample from the channel
    public:
        AdcInputChannel(int d) : currentSample(d) {} // Constructor (currentSample = d)

        // Used to request a sample from the sample channel:
        double getCurrentSample() {
            return (double)2*currentSample; // Returns 2 times the number of channel
        }

}; // End Class AdcInputChannel

// Class for the Lock/Unlock condition of the ADC
class Lock {
    private:
        bool open; // Whether ADC is available or not
    public:
        Lock() : open(true) {} // Constructor

        // Returns a flag to indicate when a thread should be blocked:
        bool isADCLocked() {
            if (open == true) {  // If ADC is available, DO NOT BLOCK the thread
                return false;
            }
            else if (open == false) { // If ADC is unavailable, BLOCK the thread
                return true;
            }
        }

        // Unlock the ADC by setting open to TRUE
        void UnlockADC() {
            open = true;
        }

        // Lock the ADC by setting open to FALSE
        void LockADC() {
            open = false;
        }
}; // End class Lock


// Class for the ADC
class ADC {
    private:
        Lock theADCLock;  // Instance for the Lock/Unlock condition
        int sampleChannel; // Channel to sample
        std::vector<AdcInputChannel>& adcchannels; // Vector of ADC channels
        std::mutex ADC_mu; // Mutex for ADC
    public:
        // Constructor: initialises a vector of ADCInputChannels
        ADC(std::vector<AdcInputChannel>& channels) : adcchannels(channels) {}

        // Request access to a channel from the ADC
        void requestADC(int c) {
            std::unique_lock<std::mutex> lock1(ADC_mu); // Unique_lock for mutual exclusion
            if (theADCLock.isADCLocked()) { // ADC locked
                std::cout << "ADC is locked, thread " << c << " is about to suspend.." << endl;
                while (theADCLock.isADCLocked()) {  // Suspend the thread requesting until ADC available
                    cond.wait(lock1);
                }
            }
            theADCLock.LockADC();  // When ADC available, gain access and lock it
            sampleChannel = c; // Channel to sample is equal to the thread ID, each thread different channel
            std::cout << "ADC locked by thread " << c << endl;
        }

        // Sample a channel, get the value
        double sampleADC(int id) {
            std::unique_lock<std::mutex> lock1(ADC_mu); // Unique_lock for mutual exclusion
            std::cout << "     sample value from thread " << id << " = " << adcchannels[sampleChannel].getCurrentSample() << endl;
            return adcchannels[sampleChannel].getCurrentSample(); // Return the sample value
        }

        // Release the ADC
        void releaseADC(int id) {
            std::unique_lock<std::mutex> lock1(ADC_mu); // Unique_lock for mutual exclusion
            std::cout << "  ADC unlocked by thread " << id << endl;
            theADCLock.UnlockADC(); // Unlock the ADC, to allow access by other threads
            cond.notify_all(); // Notify the other threads that they can access the ADC now
        }

}; // End class ADC

// Class for the Receiver
class Receiver {
     public:
        // Constructor:
        Receiver () {
            // Initialise dataBlocks 2-D array:
            for (int i = 0; i < MAX_NUM_OF_THREADS; i++) { // Rows
                for (int j = 0; j < DATA_BLOCK_SIZE; j++) { // Columns
                    dataBlocks[i][j] = 0; // Initialise to 0
                }
            }
        }

        // Receives a block of doubles such that the data
        // is stored in index id of dataBlocks[][]
        void receiveDataBlock(int id, double data[]) {
            for (int a=0; a<DATA_BLOCK_SIZE; a++) {
                dataBlocks[id][a] = data[a]; // Complete columns with data from the thread(id) specified/sending the data
            }
        }

        // Print out all items of the 2-D array
        void printBlocks() {
            for (int i = 0; i < MAX_NUM_OF_THREADS; i++) { // Rows
                std::cout << "Thread " << i << "  ";
                for (int j = 0; j < DATA_BLOCK_SIZE; j++) { // Columns
                    std::cout << dataBlocks[i][j] << ' ';
                }
                std::cout << endl; // Add new line for next thread to be displayed
            }
        }

     private:
        double dataBlocks[MAX_NUM_OF_THREADS][DATA_BLOCK_SIZE]; // 2-D array for the data

}; // End class Receiver

// Class for a Link
class Link {
    public:
         // Constructor
         Link (Receiver& r, int linkNum) : inUse(false), myReceiver(r), linkId(linkNum) {}

         // Check if the link is currently in use
         bool isInUse() {
            return inUse;
         }

         // Set the link status to busy
         void setInUse() {
            inUse = true;
         }

         // Set the link status to idle
         void setIdle() {
            inUse = false;
         }

         // Write data sampled from the thread to the Receiver through the Link
         void writeToDataLink(int id, double data[]) {
            myReceiver.receiveDataBlock(id,data); // Send data to Receiver
         }

         // Returns the link Id
         int getLinkId() {
            return linkId;
         }

    private:
         bool inUse; // Whether the Link is in Use
         Receiver& myReceiver; // Receiver reference
         int linkId; // Link ID
}; // End class Link

// Class for the LinkAccessController
class LinkAccessController {
    public:
        // Constructor
        LinkAccessController(Receiver& r) : myReceiver(r), numOfAvailableLinks(NUM_OF_LINKS) {
            for (int i = 0; i < NUM_OF_LINKS; i++) { // Add individual Links as elements in the vector of Communication Links
                commsLinks.push_back(Link(myReceiver, i));
            }
        }

        // Returns true is all Links are in use
        bool allLinksInUse () {
            if (commsLinks[0].isInUse() && commsLinks[1].isInUse() && commsLinks[2].isInUse()) {
                return true;
            }
            else return false;
        }

         //Request a Communications Link: returns an available Link.
         //If none are available, the calling thread is suspended.
        Link requestLink(int k) {
            std::unique_lock<std::mutex> lock1(LAC_mu); // Mutual Exclusion
            if (allLinksInUse()) {
                std::cout << "LINKS are locked, thread " << k << " is about to suspend.." << endl;
                while (allLinksInUse()) { // Thread suspended until a Link becomes available
                    cond.wait(lock1);
                }
            }

            if (!(commsLinks[0].isInUse())) {  // If Link 1 is free
                linkNum = 0;
                commsLinks[0].setInUse();  // Lock the Link
                std::cout << "Link " << commsLinks[0].getLinkId() << " locked by thread " << k << endl;
            }
            else if (!(commsLinks[1].isInUse())) {
                linkNum = 1;
                commsLinks[1].setInUse(); // Lock the Link
                std::cout << "Link " << commsLinks[1].getLinkId() << " locked by thread " << k << endl;
            }
            else if (!(commsLinks[2].isInUse())) {
                linkNum = 2;
                commsLinks[2].setInUse(); // Lock the Link
                std::cout << "Link " << commsLinks[2].getLinkId() << " locked by thread " << k << endl;
            }

            return commsLinks[linkNum]; // Return the available Link
        }

         //Release a Communications Link:
        void releaseLink(Link& releasedLink, int id) {
            std::unique_lock<std::mutex> lock1(LAC_mu);  // Mutual Exclusion
            number_to_release = releasedLink.getLinkId(); // Which Link is released
            commsLinks[number_to_release].setIdle(); // Unlock Link
            std::cout << "  Link " << number_to_release << " unlocked by thread " << id << endl;
            cond.notify_all(); // Notify all other threads
        }

    private:
        Receiver& myReceiver; // Receiver reference
        int numOfAvailableLinks;
        int linkNum; // Number to identify a Link
        int number_to_release; // Number of Link to be released
        std::vector<Link> commsLinks; // Vector of Communication Links
        std::mutex LAC_mu; // Mutex for LAC

}; // End class LinkAccessController

// Add a pair of thread_ID and int value on the map
void accessmap (int id){
    threadIDs.insert(std::make_pair(std::this_thread::get_id(), id));
}

// Search for a thread_ID on the map and return its int value
int search_id_function () {
    std::unique_lock<std::mutex> map_locker(mu); // Mutual Exclusion
    std::map <std::thread::id, int>::iterator it = threadIDs.find(std::this_thread::get_id()); // Iterator to search the map
    if (it == threadIDs.end()) { // If thread_id is not found
        return -1; // Return error
    }
    else {
        return it->second; // If thread_id is found, return its int value
    }
    map_locker.unlock(); // Unlock the mutex
}



// Run function executed by each thread:
void run(ADC& theADC, LinkAccessController& theLinkController, int id) {
    accessmap(id); // Add the ID of the current thread with its int value on the map
    int thread_id = search_id_function(); // Return thread's int value
    double sample; // Sample value
    // Array to store the sampled data from the thread
    double sampleBlock[DATA_BLOCK_SIZE] = {0.0}; // Initialise all elements to 0
    for (int i=0; i<DATA_BLOCK_SIZE; i++) { // Until the array becomes full of data
            // Request use of the ADC, Channel to sample is id
            theADC.requestADC(thread_id);
            // Sample the ADC and save the value
            sample = theADC.sampleADC(thread_id);
            // Save the sample in the array
            sampleBlock[i]=sample;
            // Release the ADC
            theADC.releaseADC(thread_id);
            // Delay for random period between 0.1s – 0.5s:
            std::mt19937 gen(time(0));  // Mersenne Twister pseudo-random generator seeded by time(0) (number of seconds since 1/1/1970)
            std::uniform_int_distribution<> dis(100,500); // Generate a random integer between 100 and 500 milliseconds (0.1s-0.5s)
            random_delay_between_100_and_500_i = dis(gen); // The random period is stored in a variable
            std::this_thread::sleep_for(std::chrono::milliseconds(random_delay_between_100_and_500_i)); // Random sleep time between 0.1s and 0.5s
            // Request access to a Link, print out link id and thread id
            Link link = theLinkController.requestLink(thread_id);
            // Once link obtained delay for random period between 0.1s - 0.5s
            std::mt19937 gen1(time(0)); // Mersenne Twister pseudo-random generator seeded by time(0) (number of seconds since 1/1/1970
            std::uniform_int_distribution<> dis1(100,500); // Generate a random integer between 100 and 500 milliseconds (0.1s-0.5s)
            random_delay_between_100_and_500_ii = dis1(gen1); // The random period is stored in a variable
            std::this_thread::sleep_for(std::chrono::milliseconds(random_delay_between_100_and_500_ii)); // Random sleep time between 0.1s and 0.5s
            // Transmit data to the Receiver
            link.writeToDataLink(thread_id,sampleBlock);
            // Release Link, print thread id and the link id of the Link that was released
            theLinkController.releaseLink(link, thread_id);
    }
} // End of Run function

// Main function
int main() {

    // Initialise the ADC channels
    std::vector<AdcInputChannel> adcChannels;
    for (int k= 0; k < MAX_NUM_OF_CHAN; k++) {
        adcChannels.push_back(AdcInputChannel(k)); // Each AdcInputChannel is initialised with a different value
    }

    // Instantiate the ADC
    ADC theadc(adcChannels);

    // Instantiate the Receiver
    Receiver theReceiver;

    // Instantiate the LinkAccessController
    LinkAccessController lac(theReceiver);

    // Instantiate and start the threads
    std::thread the_threads[MAX_NUM_OF_THREADS]; // Array of threads

    for (int i = 0; i < MAX_NUM_OF_THREADS; i++) {
        // Launch the threads
        the_threads[i] = std::thread(run, std::ref(theadc), std::ref(lac),i); // Threads execute run function
    }

    // Wait for the threads to finish
    for (int n = 0; n < MAX_NUM_OF_THREADS; n++) {
        the_threads[n].join();
    }

    // Add new lines to make the result clearly visible
    std::cout << endl << endl << endl;

    // Print out the data in the Receiver
    std::cout << "          Table of data in Receiver        " << endl; // Header Line 1
    std::cout << "-------------------------------------------" << endl; // Header Line 2
    theReceiver.printBlocks(); // Print the 2-D array of data in the Receiver
    std::cout << endl << endl; // New lines for visibility

    cout << "All threads terminated" << endl; // Message for End of Threads

    return 0;

} // End of Main function
