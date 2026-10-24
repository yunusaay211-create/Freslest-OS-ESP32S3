"""Real Tk viewport and input regression. Requires a display (Xvfb works)."""
from pathlib import Path
import importlib.util,sys,tkinter as tk
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'PC-Viewer'))
spec=importlib.util.spec_from_file_location('gui_viewer',r/'PC-Viewer/freslest_viewer.py');v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
root=tk.Tk();viewer=v.Viewer(root);root.geometry('700x500');root.update()
try:
 for w,h in ((480,320),(480,360),(240,160),(240,180)):
  viewer.image=tk.PhotoImage(width=w,height=h);viewer.image.put('#00cc00',to=(0,0,w,h));viewer.image.put('#ff0000',to=(0,0,w,8));viewer.image.put('#0000ff',to=(0,h-8,w,h))
  viewer.frame_size=(w,h);viewer.divisor=480//w;viewer.show_image(reset=True);root.update_idletasks()
  box=viewer.canvas.bbox(next(i for i in viewer.canvas.find_all() if viewer.canvas.type(i)=='image'))
  assert box[0]>=0 and box[1]>=0 and box[2]<=viewer.canvas.winfo_width() and box[3]<=viewer.canvas.winfo_height(),(w,h,box)
  assert viewer.display_image.get(0,0)==(255,0,0) and viewer.display_image.get(viewer.display_image.width()-1,viewer.display_image.height()-1)==(0,0,255)
  assert viewer.point(type('E',(),{'x':box[0],'y':box[1]}))==(0,0)
 viewer.zoom.set('3x');viewer.show_image(reset=True);root.update_idletasks();viewer.canvas.yview_moveto(1);root.update_idletasks()
 assert viewer.canvas.canvasy(0)>0 and viewer.canvas.bbox(next(i for i in viewer.canvas.find_all() if viewer.canvas.type(i)=='image'))[3]>viewer.canvas.winfo_height()
 viewer.zoom.set('Fit');viewer.show_image(reset=True);root.update_idletasks();viewer.connected=True
 x,y=viewer.layout[:2];viewer.canvas.event_generate('<ButtonPress-1>',x=x+10,y=y+10);viewer.canvas.focus_force();root.update();assert viewer.controls.snapshot()[3]==1
 viewer.canvas.event_generate('<ButtonRelease-1>',x=0,y=0);root.update();assert viewer.controls.snapshot()[3]==0
 viewer.canvas.event_generate('<KeyPress>',keysym='Left');root.update();assert viewer.controls.snapshot()[1][10]&1
 viewer.ports.focus_force();root.update();assert v.control_commands(viewer.controls.snapshot())==b'FSR\n'
 print('PASS: real Tk Fit shows top/footer bars at all four sizes; zoom scrolls; real canvas mouse, keyboard and focus release work')
finally:viewer.close()
