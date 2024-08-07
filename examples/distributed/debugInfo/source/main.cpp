#include <taskr/taskr.hpp>

int main(int argc, char **argv)
{
  // Initializing taskr
  taskr::Runtime taskr;

  // Getting distributed instance information
  const auto instanceCount = taskr.getInstances().size();
  const auto myInstanceId = taskr.getCurrentInstance()->getId();
  const auto rootInstanceId = taskr.getRootInstanceId();

  // Printing my instance info
  printf("Instance %03lu / %03lu %s\n", myInstanceId, instanceCount, myInstanceId == rootInstanceId ? "(Root)" : "");

  return 0;
}
