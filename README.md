# Concurrent Prefix Sum

## The Task

For this assignment, I was tasked with implementing an algorithm that calculates a prefix sum using concurrency techniques. Specifically, I was allowed to use C's pthreads and their associated locks, condition variables, and/or semaphores. There was one more restriction: there could not be any sequential dependencies in my implementation. I cannot have a situation where my algorithm needed a thread to wait for another thread, which also needed to wait for another thread to finish.

In addition to test cases, which tested the correctness of my implementation's output, I was also given a serial prefix solution as a benchmark for my algorithm's run time. I was given a range of 2.0X (2 times slower than the serial solution) to 3.0X to aim for with my implementation. 

## My Approach

The recommended approach for this assignment was to implement the Hillis Steele scan, which used a combination of barriers and locks to use multiple threads in calculating the prefix sum. Other than that algorithm, the Blelloch scan was also suggested, although that implementation was a bit too complex for this assignment.

I decided to do neither of these approaches and forged my own path. I designed a divide and conquer algorithm for this assignment that took advance of the fact that multiple serial prefix sums can be performed at the same time and that these prefix sums can pass each other values to slowly build the full prefix sum. I ended up writing a [write-up of my algorithm's design which covered its design, implementation, and how it maximizes concurrency opportunities](https://github.com/apietrick24/Concurrent-Prefix-Sum/blob/main/Concurrent%20Prefix%20Sum%20Writeup.pdf). I highly recommend you read/skim it if you are interested. I also included a step-by-step example of how it works.

## Obstacles 

I found that concurrency was quite hard for me to implement properly at first. Most of the time I spent on this assignment was debugging issues that arose due to the concurrent sections of my algorithm. While my implementation only uses lock and condition variables to prevent multiple threads from entering the a critical zone at the same time, I still ran into quite a lot of problems with threads accidentally accessing the same resource at the same time. I also ended up deadlocking my program a few times by mistake. 

Despite the many hiccups along the way, I forged through these issues.  By the end of the assignment, I learned to respect how fascinating concurrency truly was and how it can be used to greater improve the run times of programs. It is definetely a topic and tool I have like to employ in my programs.

## Result

I am quite proud of my algorithm's implementation. It averages ~1.05X, tieing for the fastest implementation in the class. I really enjoyed every aspect of this project, especially the algorithm design portion, and I definetely want to tackle a similar challenge in the future. 
