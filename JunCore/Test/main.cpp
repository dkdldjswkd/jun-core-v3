#include <iostream>
#include "../JunCommon/lib/LFQueue.h"
using namespace std;

int main()
{
	LFQueue<int> lfq;
	lfq.Enqueue(1);
	lfq.Enqueue(2);
	lfq.Enqueue(3);

	int a;

	for (; lfq.Dequeue(&a);)
	{
		cout << a << endl;
	}
}