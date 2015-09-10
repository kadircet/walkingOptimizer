import dy
from time import time
import ctypes

NETWORKINTERVAL = .1
def typetostr(dtype):
	if dtype == dy.DY_INT8:
		return "DY_INT8"
	if dtype == dy.DY_UINT8:
		return "DY_UINT8"
	if dtype == dy.DY_INT16:
		return "DY_INT16"
	if dtype == dy.DY_UINT16:
		return "DY_UINT16"
	if dtype == dy.DY_INT32:
		return "DY_INT32"
	if dtype == dy.DY_UINT32:
		return "DY_UINT32"
	if dtype == dy.DY_FLOAT:
		return "DY_FLOAT"
	if dtype == dy.DY_DOUBLE:
		return "DY_DOUBLE"
		
class dyData:
	def __init__(self, name, data=None, dtype=dy.DY_FLOAT, pull=False):
		self.name = name
		self.data = data
                self.pull = pull
		self.dataP = dy.data.retrieve(name)
                if bool(self.dataP)==0:
			self.dataP=dy.data.create(dtype, name)
		self.type = self.dataP.contents.dtype
		self.setter = dyData.getSetter(self.type)
		self.getter = dyData.getGetter(self.type)
		if data or data==0:
			self.set(data)
		dy.network.push(name)
	
	@staticmethod
	def getSetter(dtype):
		if (dtype>=dy.DY_INT8 and dtype<=dy.DY_UINT32) or (dtype==dy.DY_CHAR or dtype==dy.DY_UCHAR):
			return dy.data.set_int
		if dtype==dy.DY_INT64:
			return dy.data.set_int64
		if dtype==dy.DY_UINT64:
			return dy.data.set_uint64
		if dtype==dy.DY_FLOAT:
			return dy.data.set_float
		if dtype==dy.DY_DOUBLE:
			return dy.data.set_double
		raise Exception("Unsupported data type %#x"%dtype)
		
	@staticmethod
	def getGetter(dtype):
		if (dtype>=dy.DY_INT8 and dtype<=dy.DY_UINT32) or (dtype==dy.DY_CHAR or dtype==dy.DY_UCHAR):
			return dy.data.get_int
		if dtype==dy.DY_INT64:
			return dy.data.get_int64
		if dtype==dy.DY_UINT64:
			return dy.data.get_uint64
		if dtype==dy.DY_FLOAT:
			return dy.data.get_float
		if dtype==dy.DY_DOUBLE:
			return dy.data.get_double
		raise Exception("Unsupported data type %#x"%dtype)
			
	def set(self, data):
		self.setter(self.dataP, data)
                dy.network.push(self.name)
	
	def get(self):
            if self.pull:
                dy.network.pull(self.name)
	    self.data = self.getter(self.dataP)
	    return self.data
	
	def __str__(self):
		return "{0}({1})".format(typetostr(self.type), self.data)
		
	def __repr__(self):
		return "{0}({1})".format(typetostr(self.type), self.data)
		
class dySet:
	def __init__(self, path='', data=None, spull=False):
		self.path=path
		prefix = self.path
		self.map = {}
		if data:
			self.data = data
		elif path=='':
			self.data = dy.dy_dataset_p.in_dll(dy.dlib, "dy_data_toplevel").contents
		else:
			self.data = dy.data.path(path).contents
		if prefix!='':
			prefix=prefix+'.'
		for i in range(self.data.size):
			if bool(self.data.table[i]):
				elm = self.data.table[i][0]
				if elm.type==0:
					data = ctypes.cast(elm.item, dy.dy_dataset_p).contents
					self.map[elm.key]=dySet(prefix+elm.key, data)
				else:
					data = ctypes.cast(elm.item, dy.dy_data_p).contents
					self.map[elm.key]=dyData(prefix+elm.key, dyData.getGetter(data.dtype)(data),pull=spull)
					
	def __repr__(self):
		return str(self.map)
		
	def __str__(self):
		return str(self.map)

