import sys,unittest,tkinter as tk
from pathlib import Path
p=Path(sys.argv.pop(1)).resolve() if len(sys.argv)>1 and not sys.argv[1].startswith('-') else Path(__file__).resolve().parents[1]
sys.path.insert(0,str(p))
import freslest_viewer as v

class ViewerPresentation(unittest.TestCase):
 def setUp(self):
  self.root=tk.Tk();self.viewer=v.Viewer(self.root);self.root.geometry('700x500');self.root.update_idletasks()
  # Keep the actual widgets and rendering; remove automatic connection timers.
  for item in self.root.tk.splitlist(self.root.tk.call('after','info')):self.root.after_cancel(item)
  self.viewer.connected=True;self.viewer.connection_label='NATIVE test'
 def tearDown(self):self.viewer.close()
 def frame(self,seq,r=255,g=0,b=0):
  return 480,320,b'P6\n480 320\n255\n'+bytes((r,g,b))*480*320,seq
 def test_one_pump_presents_only_the_newest_frame(self):
  painted=[];real=self.viewer.show_image
  def draw(*a,**kw):painted.append(1);return real(*a,**kw)
  self.viewer.show_image=draw
  for i in range(100):self.viewer.post('frame',self.frame(i,i,0,0))
  self.viewer.pump();self.root.update_idletasks()
  self.assertEqual(len(painted),1,'queued old frames consume the Tk event loop')
  self.assertEqual(self.viewer.image.get(0,0),(99,0,0))
 def test_frame_updates_reuse_image_and_canvas_resources(self):
  self.viewer.post('frame',self.frame(1));self.viewer.pump();self.root.update_idletasks()
  original=self.viewer.image;scaled=self.viewer.display_image;items=self.viewer.canvas.find_all()
  self.viewer.post('frame',self.frame(2,0,0,255));self.viewer.pump();self.root.update_idletasks()
  self.assertIs(self.viewer.image,original,'base Tk image was reallocated')
  self.assertIs(self.viewer.display_image,scaled,'scaled Tk image was reallocated')
  self.assertEqual(self.viewer.canvas.find_all(),items,'canvas items were recreated')
  self.assertEqual(self.viewer.image.get(0,0),(0,0,255))
 def test_frame_burst_does_not_drop_disconnect_or_connected_state(self):
  self.viewer.post('lost',None)
  for i in range(100):self.viewer.post('frame',self.frame(i))
  self.viewer.pump();self.assertFalse(self.viewer.connected,'frame flood re-enabled a disconnected session')

if __name__=='__main__':unittest.main()
