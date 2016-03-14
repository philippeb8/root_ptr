# root_ptr
Deterministic Region Based Memory Manager

For the documentation, please refer to:
http://htmlpreview.github.io/?https://github.com/philippeb8/root_ptr/blob/master/doc/index.html


The latest benchmark:

unique_ptr (new): 43.2705
unique_ptr (make_unique): 42.4111
shared_ptr (new): 68.9101
shared_ptr (make_shared): 46.6575
shared_ptr (allocate_shared_noinit): 31.2334
root_ptr (new): 30.3701


Regards,
-Phil
