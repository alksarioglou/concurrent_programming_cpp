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

//global constants:
int const MAX_NUM_OF_CHAN = 6; //number of AdcInputChannels
int const MAX_NUM_OF_THREADS = 6;
int const DATA_BLOCK_SIZE = 20;

int random_delay_between_100_and_500 = 0;

//global variables:
std::mutex mu;
std::condition_variable cond;
std::unique_lock<std::mutex> lock1;
std::unique_lock<std::mutex> map_locker;

// Map of threads IDs and int
std::map<std::thread::id, int> threadIDs;
//function prototypes: (if used)
//....

class AdcInputChannel {
    private:
        int currentSample;
    public:
        AdcInputChannel(int d) : currentSample(d) {} //constructor syntax for 'currentSample=d'
        //used to request a sample from the sample channel:
        double getCurrentSample() {
            return (double)2*currentSample;
        }
}; //end class AdcInputChannel

class Lock {
    private:
        bool open;
    public:
        Lock() : open(true) {}//constructor
        //returns a flag to indicate when a thread should be blocked:
        bool isADCLocked() {
            if (open == true) {
                return false;
            }
            else if (open == false) {
                return true;
            }
        }

        void UnlockADC() {
            open = true;
        }

        void LockADC() {
            open = false;
        }
}; //end class Lock


class ADC {
    private:
        Lock theADCLock;
        int sampleChannel;
        bool displaying;
        std::vector<AdcInputChannel>& adcchannels; //vector reference
        std::mutex ADC_mu; //mutex
    public:
        //constructor: initialises a vector of ADCInputChannels
        //passed in by reference:
        ADC(std::vector<AdcInputChannel>& channels) : adcchannels(channels) {}

        void requestADC(int c) {
            std::unique_lock<std::mutex> lock1(ADC_mu);
            if (theADCLock.isADCLocked()) {// ADC locked
                std::cout << "ADC is locked, thread " << c << " is about to suspend.." << endl;
                while (theADCLock.isADCLocked()) {
                    cond.wait(lock1);
                }
            }
            theADCLock.LockADC();
            sampleChannel = c;
            std::cout << "ADC locked by thread " << c << endl;
        }

        double sampleADC() {
            std::unique_lock<std::mutex> lock1(ADC_mu);
            return adcchannels[sampleChannel].getCurrentSample();
        }

        void releaseADC() {
            std::unique_lock<std::mutex> lock1(ADC_mu);
            theADCLock.UnlockADC();
            cond.notify_all();
        }

}; //end class ADC


//run function –executed by each thread:
void run(ADC& theADC, int id) {
    double sample;
    //to store the sampled data: (for part A2 only)
    double sampleBlock[DATA_BLOCK_SIZE] = {0.0}; //initialise all elements to 0
    for (int i=0; i<50; i++) { //replace with 'i<DATA_BLOCK_SIZE;' for A2
            // request use of the ADC; channel to sample is id:
            theADC.requestADC(id);
            // sample the ADC:
            sample = theADC.sampleADC();
            std::cout << "     sample value from thread " << id << " = " << sample << endl;
            // release the ADC:
            std::cout << "  ADC unlocked by thread " << id << endl;
            theADC.releaseADC();
            // delay for random period between 0.1s – 0.5s:
            std::mt19937 gen(time(0));
            std::uniform_int_distribution<> dis(100,500);
            random_delay_between_100_and_500 = dis(gen);
            std::this_thread::sleep_for(std::chrono::milliseconds(random_delay_between_100_and_500));
    }
}

void accessmap (int id){
    threadIDs.insert(std::make_pair(std::this_thread::get_id(), id));
    //cout << "Thread: " << std::this_thread::get_id() << " , ID: " << id << endl;
}

int search_function (int iden) {
    std::unique_lock<std::mutex> map_locker(mu);
    accessmap(iden);
    std::map <std::thread::id, int>::iterator it = threadIDs.find(std::this_thread::get_id());
    if (it == threadIDs.end()) {
        //cout << "Int value not found" << endl;
        return -1;
    }
    else {
        //cout << "For thread: " << std::this_thread::get_id() << " , ID: " << it->second << endl;
        return it->second;
    }
    cond.notify_all();
}

int main() {

    //initialise the ADC channels:
    std::vector<AdcInputChannel> adcChannels;
    for (int k= 0; k < MAX_NUM_OF_CHAN; k++) {
        AdcInputChannel adc_channel(k); //each AdcInputChannel is initialised with a different value
        adcChannels.push_back(adc_channel);
    }

    // Instantiate the ADC:
    ADC theadc(adcChannels);
    //instantiate and start the threads:

    std::thread the_threads[MAX_NUM_OF_THREADS]; //array of threads

    for (int i = 0; i < MAX_NUM_OF_THREADS; i++) {
        //launch the threads:
        the_threads[i] = std::thread(run, std::ref(theadc),i);
    }

    //wait for the threads to finish:
    for (int n = 0; n < MAX_NUM_OF_THREADS; n++) {
        the_threads[n].join();
    }

    cout << "All threads terminated" << endl;

    return 0;

}
