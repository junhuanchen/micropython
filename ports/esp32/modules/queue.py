from heapq import heappush, heappop

class PriorityQueue:
	def __init__(self):
		self._queue = []

	def put(self, item, priority):
		heappush(self._queue, (-priority, item))

	def get(self):
		return heappop(self._queue)[-1]
		
if __name__ == '__main__':
	q = PriorityQueue()
	q.put('world', 1)
	q.put('hello', 2)
	print(q.get())
	print(q.get())