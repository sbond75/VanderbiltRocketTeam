// https://stackoverflow.com/questions/43244372/how-to-use-the-raspberry-pi-camera-as-video-input-in-c-opencv
#include <iostream>
#include<opencv2/opencv.hpp>

// https://stackoverflow.com/questions/1641182/how-can-i-catch-a-ctrl-c-event
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// C++20 required:
//#include <semaphore>
#include "Semaphore.h"
#include <cstring>
#include <thread>

static unsigned int counter = 0;
namespace std {
  // Wrapper around Semaphore class from Semaphore.h to replicate parts of the C++20 binary_semaphore's interface.
  class binary_semaphore {
  public:
    binary_semaphore(unsigned int value) :
      name(std::string("/") + std::to_string(counter++)),
      s(const_cast<char*>(name.c_str()), value)
    {}

    void acquire() {
      int retval = s.wait();
      std::cout << name << ": acquire returned " << retval << std::endl;
      if (retval != 0) {
	std::cout << std::strerror(errno) << std::endl;
	exit(1);
      }
    }

    void release() {
      int retval = s.post();
      std::cout << name << ": release returned " << retval << std::endl;
      if (retval != 0) {
	std::cout << std::strerror(errno) << std::endl;
	exit(1);
      }
    }
    
  private:
    std::string name;
    Semaphore s;
  };
}

// global binary semaphore instances
// object counts are set to zero
// objects are in non-signaled state
std::binary_semaphore
	smphSignalMainToThread(0),
	smphSignalThreadToMain(0);

cv::VideoWriter writer;

void my_handler(int s){
           printf("Caught signal %d\n",s);
	   
	   // wait until the worker thread is done doing the work
	   // by attempting to decrement the semaphore's count
	   smphSignalThreadToMain.acquire();

	   std::cout << "[main: ctrl-c handler] Got the signal\n"; // response message

	   // Save video
	   writer.release();
	   
           exit(0);
}

int main(int argc, char** argv)
{
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = my_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);


  cv::Mat output;
  cv::VideoCapture cap(cv::CAP_ANY);

  
  if( !cap.isOpened() )
    {
      std::cout << "Could not initialize capturing...\n";
      return 1;
    }

  // Get first frame
  cap >> output;
  bool isColor = (output.type() == CV_8UC3);
  std::cout << "Started writing video... " << std::endl;
  
  // https://learnopencv.com/how-to-find-frame-rate-or-frames-per-second-fps-in-opencv-python-cpp/
  // "For OpenCV 3, you can also use the following"
  double fps = cap.get(cv::CAP_PROP_FPS);
  double width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
  double height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
  std::cout << "Frames per second using video.get(CAP_PROP_FPS) : " << fps << "\nType: " << output.type() << "\nWidth: " << width << "\nHeight: " << height << std::endl;

  // https://stackoverflow.com/questions/60204868/how-to-write-mp4-video-with-opencv-c
  int codec = cv::VideoWriter::fourcc('a', 'v', 'c', '1');
  //double fps = 29.97;
  std::string filename = "live2.mp4";
  cv::Size sizeFrame(640,480);
  writer.open(filename, codec, fps, sizeFrame, isColor);
  
  constexpr int64 kTimeoutNs = 1000;
  std::vector<int> ready_index;
  cv::Mat xframe;
  //std::atomic<bool> frameReady = false;
  std::thread videoCaptureThread([&](){
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point now;
    long m_duration = 0;
    while(1) {
      // // https://stackoverflow.com/questions/62609167/how-to-use-cvvideocapturewaitany-in-opencv
      // // "VideoCapture::waitAny will wait for the specified timeout (1 microsecond) for the camera to produce a frame, and will return after this period. If the camera is ready, it will return true (and will also populate ready_index with the index of the ready camera. Since you only have a single camera, the vector will be either empty or non-empty)."
      // if (cv::VideoCapture::waitAny({cap}, ready_index, kTimeoutNs // "If a timeout is not provided (or equivalently, if it's set to 0), then the function blocks indefinitely until the frame is available. In a single-camera case, it is then similar to VideoCapture::grab()."
      // 			      )) {
      //   // Camera was ready; get image.
      //   //cap.retrieve(image);

      //   cap >> output;

      //   imshow("webcam input", output);
      //   char c = (char)cv::waitKey(10);
      //   if( c == 27 ) break;
      // } else {
      //   // Camera was not ready; do something else. 
      // }

      // https://stackoverflow.com/questions/64173224/c-opencv-videowriter-frame-rate-synchronization
      now = std::chrono::steady_clock::now();
      long duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();
      double dest = (1e9 / fps);
      if ((double)duration > dest) {
	std::cout << "time off by: " << duration - dest << std::endl;
	start = std::chrono::steady_clock::now();
	m_duration += duration;

	// wait for a signal from the main proc
	// by attempting to decrement the semaphore
	smphSignalMainToThread.acquire();
 
	// this call blocks until the semaphore's count
	// is increased from the main proc
 
	std::cout << "[thread] Got the signal\n"; // response message
	


	//cap >> output;
	
	// Capture frame-by-frame
        if(cap.grab()) { cap.retrieve(output); } else { std::cout << "[thread] Continue\n"; continue; }
    
	cv::resize(output,xframe,sizeFrame);
	writer.write(xframe);

      
      
	std::cout << "[thread] Send the signal\n"; // message
 
	// signal the main proc back
	smphSignalThreadToMain.release();
      }
      else {
	std::cout << "[thread] Duration not ready\n";
      }
    }
  });
  while(1){
    
    std::cout << "[main] Send the signal\n"; // message
 
    // signal the worker thread to start working
    // by increasing the semaphore's count
    smphSignalMainToThread.release();
    
    // imshow("webcam input", output);
    // char c = (char)cv::waitKey(10);
    // if( c == 27 ) break;

    // wait until the worker thread is done doing the work
    // by attempting to decrement the semaphore's count
    smphSignalThreadToMain.acquire();
 
    std::cout << "[main] Got the signal\n"; // response message
  }
}
