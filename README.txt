Author: Lai Man Tang, Yuting Tan
Date: 27/11/2017

This project aimed to solve the reader writer problem. readerTree and RWQueue are implemented to handle the thread.


readerTree is a Tree Structure that stored the incoming reader thread. RWQueue is a reader writer thread manager that managing the thread instance adding and leaing the tree.

the first incoming thread call the tree constructor. The tree constructor will locked by the RWQueue. Thus, the tree constructor will happend before addReader function.
