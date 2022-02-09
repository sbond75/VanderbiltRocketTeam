#include "subscaleMain.hpp"

#include <python3.7m/Python.h>
#include "VADL2022.hpp"
#include <thread>
#include "py.h"

Queue<QueuedFunction, 8> mainDispatchQueue;
Queue<QueuedFunction, 16> radioDispatchQueue;
std::atomic<bool> isRunningPython = false;
std::atomic<bool> mainDispatchQueueRunning = false;
std::atomic<bool> mainDispatchQueueDrainThenStop = false;
std::unique_ptr<std::thread> videoCaptureOnlyThread;

int main(int argc, char **argv) {
  VADL2022 vadl(argc, argv);

  std::thread th { [](){
    QueuedFunction f;
    std::cout << "Started radio thread" << std::endl;
    do {
      std::cout << "Radio thread waiting for next function..."
                << std::endl;
      radioDispatchQueue.dequeue(&f);
      std::cout << "Radio thread executing function with description: " << f.description << std::endl;
      //f.f(); // TODO: implement properly
    } while (mainDispatchQueueRunning);
  }};

  QueuedFunction f;
  mainDispatchQueueRunning = true;
  do {
    std::cout << "Main dispatch queue waiting for next function..." << std::endl;
    mainDispatchQueue.dequeue(&f);
    std::cout << "Main dispatch queue executing function with description: " << f.description << std::endl;
    if (f.type == QueuedFunctionType::Python) {
      isRunningPython = true;
    }
    f.f();
    if (f.type == QueuedFunctionType::Python) {
      isRunningPython = false;
    }
    if (mainDispatchQueueDrainThenStop && mainDispatchQueue.empty()) {
      std::cout << "Drained queue, now stopping" << std::endl;
      mainDispatchQueueRunning = false;
    }
  } while (mainDispatchQueueRunning);

  std::cout << "Main dispatch queue no longer running" << std::endl;

  //std::cout << "Waiting for radio thread to finish..." << std::endl; // TODO: implement properly
  //th.join(); // TODO: implement properly
  std::cout << "Radio thread finished" << std::endl;

  if (videoCaptureOnlyThread) {
    videoCaptureOnlyThread->join();
    videoCaptureOnlyThread.reset(); // Do this here instead of outside main()/in global scope since std threads don't like to be destructor'ed in global scope
  }

  //PyRun_SimpleString("input('Done')");
  return 0;
}

void reportStatus(Status status) {
  // std::string status_str = std::to_string(status);
  // std::string cmd = "python3 subscale_driver/radio.py 0 " + status_str;
  // system(cmd.c_str());

  
  // std::cout << "Reporting status: " << status << std::endl;
  return; // TODO: implement properly
  
  const char* path = "subscale_driver/radio.py";
  radioDispatchQueue.enqueue([=](){
    //PyGILState_STATE state = PyGILState_Ensure(); // Only run this if no other python code is running now, otherwise wait for a lock // TODO: implement properly

    std::string status_str = std::to_string(status);
    const char* args[] = {"0", status_str.c_str(), NULL};
    S_RunFile(path, 2, (char**)args);
    
    //PyGILState_Release(state); // TODO: implement properly
  },path,QueuedFunctionType::Python);
}
