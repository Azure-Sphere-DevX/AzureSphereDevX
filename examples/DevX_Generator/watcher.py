import os
import sys 
import time

class Watcher(object):
    running = True
    refresh_delay_secs = 2

    # Constructor
    def __init__(self, watch_file, call_func_on_change=None):        
        self.filename = watch_file
        self._cached_stamp = os.stat(self.filename).st_mtime
        self.call_func_on_change = call_func_on_change


    # Look for changes
    def look(self):
        stamp = os.stat(self.filename).st_mtime
        if stamp != self._cached_stamp:
            self._cached_stamp = stamp
            # File has changed, so do something...
            print('File changed')
            if self.call_func_on_change is not None:
                self.call_func_on_change()

    # Keep watching in a loop        
    def watch(self):
        while self.running: 
            try: 
                # Look for changes
                time.sleep(self.refresh_delay_secs) 
                self.look() 
            except KeyboardInterrupt: 
                print('\nDone') 
                break 
            except FileNotFoundError:
                # Action on file not found
                pass
            except: 
                print('Unhandled error: %s' % sys.exc_info()[0])