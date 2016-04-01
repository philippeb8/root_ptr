# root_ptr
*Deterministic Region Based Memory Manager*



For the documentation, please refer to:

http://philippeb8.github.io/root_ptr/



*The latest benchmark (single threaded):*

unique_ptr (new): 43.2705

unique_ptr (make_unique): 42.4111

shared_ptr (new): 68.9101

shared_ptr (make_shared): 46.6575

shared_ptr (allocate_shared_noinit): 31.2334

root_ptr (new): 30.3701



*The latest benchmark (multi threaded):*

unique_ptr (new): 42.1397

unique_ptr (make_unique): 43.4631

shared_ptr (new): 76.4543

shared_ptr (make_shared): 76.6611

shared_ptr (allocate_shared_noinit): 81.9926

root_ptr (new): 66.3417 


Regards,

-Phil
