from dyExt import *
import Tkinter as tk
import ttk
import threading

tree=None
root=None
def refresh():
	insertData(tree, '', dySet('').map)
	root.after(1000, refresh)

def insertData(tree, parent, data):
	for key in data:
		if isinstance(data[key], dySet):
			if not tree.exists(data[key].path):
				tree.insert(parent, 'end', data[key].path, text=key)
			insertData(tree, data[key].path, data[key].map)
		else:
			if not tree.exists(data[key].name):
				tree.insert(parent, 'end', data[key].name, text=key, 
					value=str(data[key]))
			tree.item(data[key].name, value=str(data[key]))

class EntryPopup(tk.Entry):
    def __init__(self, parent, text, callback, **kw):
        ''' If relwidth is set, then width is ignored '''
        tk.Entry.__init__(self, parent, **kw)
        self.callback = callback
        self.parent = parent
        self.insert(0, text) 
        self['state'] = 'normal'
        self['readonlybackground'] = 'white'
        self['selectbackground'] = '#1BA1E2'
        self['exportselection'] = False

        self.focus_force()
        self.bind("<Escape>", self.sendData)
        self.bind("<Return>", self.sendData)
        self.selection_range(text.find('(')+1, text.find(')'))
        self.icursor(text.find('(')+1)
    
    def sendData(self, *ignore):
    	self.callback(eval(self.get()[self.get().find('(')+1:-1]))
    	self.parent.focus_force()
    	self.destroy()
    	return 'break'
		
def ondclick(event):
	item = tree.selection()[0]
	cid = "#1"
	pid = tree.parent(item)
	if pid=='' or len(tree.get_children(item))>0:
		return
	x,y,width,height = tree.bbox(item, cid)
	pady = height // 2
	data = tree.item(item, 'value')[0]
	print item
	popUp = EntryPopup(tree, data, lambda ndata:dyData(item, ndata))
	popUp.place(x=x, y=y+pady, anchor=tk.W, relwidth=1)
	
def main(argv):
	global tree, root
	dy.init()
	dy.network.connect(argv[1], int(argv[2]))
	root = tk.Tk()
	root.columnconfigure(0, weight=1)
	root.rowconfigure(0, weight=1)
	tf = ttk.Frame(root, padding="3")
	tf.grid(row=0, column=0, sticky=tk.NSEW)
	tree = ttk.Treeview(tf, columns=("Value"))
	tree.bind("<Double-1>", ondclick)
	tree.bind("<Return>", ondclick)
	tree.column('Value', width='200', anchor='center')
	tree.heading('Value', text='Value')
	tree.pack(fill=tk.BOTH, expand=1)
	root.after(1000, refresh)
	root.mainloop()

import sys
if __name__=='__main__':
	main(sys.argv)
 
