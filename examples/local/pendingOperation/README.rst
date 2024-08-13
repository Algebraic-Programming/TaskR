Pending Operation
=====================

A simple example that showcases TaskR's support for task-aware operations, i.e., those with asynchronous completion check. These operations suspend the task only (not the entire thread) so that other tasks can execute in its stead while the operation completes.