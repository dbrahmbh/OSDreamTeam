Team Members:

  Deep Brahmbhatt, dbrahmbh@nd.edu
  Delna Balsara, dbalsar1@nd.edu
  Andres Gonzalez, agonza42@nd.edu
  Mariana Gonzalez, mgonza32@nd.edu

Aggressive Locking Run Times**:
(Number of Consumers - Average Run Time in seconds)
1 - 48.067965
2 -  38.382528
5 - 33.149965
8 - 35.617637
10 - 40.421454

Results Discussion:
  There was a range of 33-50 for the occurences of "the". The runtimes decreased until 5 consumers, began to plateau, and then started to increase again. This is most likely because the consumer threads began to spin while waiting for work from the producers. 

Condition Variable Run Times**:
(Number of Consumers - Average Run Time in seconds)
1 - 17.312400
2 - 16.060560
5 - 19.994763
8 - 19.301076
10 - 19.721409

Results Discussion:
  There was a range of 39-42 for the occurences of "the". The times were all within a small range of values (from 16-19 seconds). The reason why is because the consumer threads do not have to spin in this variation.

** All tests are run with 10 producers and 3000 iterations.
