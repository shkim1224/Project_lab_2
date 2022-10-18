import os
import argparse
import json
import threading
import time
import numpy as np
from os.path import join
from io import BytesIO
from functools import partial
from http.server import HTTPServer, BaseHTTPRequestHandler
import scipy as sp
from scipy import stats
from numpy.linalg import norm
import matplotlib.pyplot as plt

# Settings
DEFUALT_PORT = 1337
MAX_MEASUREMENTS = 128      # Truncate measurements to this number
ANOMALY_THRESHOLD = 0.4    # An MD over this will be considered an anomaly
downsample_factor = 1
# Global flag
server_ready = 0

################################################################################
# Functions
# Function: Calculate FFT for each axis in a given sample
def extract_fft_features(sample,max_measurements):
    # Truncate sample size
    sample = sample[0:max_measurements]
    # Crate a window
    hann_window = np.hanning(sample.shape[0])

    # Compute a windowed FFT of each axis in the sample (leave off DC)
    out_sample = np.zeros((int(sample.shape[0] / 2), sample.shape[1]))
    for i, axis in enumerate(sample.T):
        fft = abs(np.fft.rfft(axis * hann_window))
        out_sample[:, i] = fft[1:]
    return out_sample

def fault_detect(x, normal_fft):
    with np.load(join("G:\second-paper-for-ahn\data_collection\\normal_abnormal.npz")) as data:
        normal_fft = data['normal_fft']
        abnormal_fft = data['abnormal_fft']
    cosine_x = np.dot(x[:,0],normal_fft[:,0])/(norm(x[:,0])*norm(normal_fft[:,0]))
    cosine_y = np.dot(x[:,1],normal_fft[:,1])/(norm(x[:,1])*norm(normal_fft[:,1]))
    cosine_z = np.dot(x[:,2],normal_fft[:,2])/(norm(x[:,2])*norm(normal_fft[:,2]))
    result_1 = [cosine_x,cosine_y,cosine_z]
    cosine = np.min(result_1)
    print("cos_x: ",cosine_x)
    print("cos_y: ",cosine_y)
    print("cos_z: ",cosine_z)
    return cosine

# Decode string to JSON and save measurements in a file
def parseSamples(json_str):
    # Create a browsable JSON document
    try:
        json_doc = json.loads(json_str)
    except Exception as e:
        print('ERROR: Could not parse JSON |', str(e))
        return
    # Parse sample
    sample = []
    num_meas = len(json_doc['x'])
    # print(num_meas)
    for i in range(0, num_meas):
        sample.append([float(json_doc['x'][i]),
                        float(json_doc['y'][i]),
                        float(json_doc['z'][i])])
    # Calculate MAD for each axis
    # print(sample)
    feature_set = extract_fft_features(np.array(sample), 
                                    max_measurements=MAX_MEASUREMENTS)
    # Compute Mahalnobis Distance between sample and model mean
    with np.load(join("G:\second-paper-for-ahn\data_collection\\normal_abnormal.npz")) as data:
        normal_fft = data['normal_fft']
        abnormal_fft = data['abnormal_fft']
      
    fault_index = fault_detect(feature_set, normal_fft)
    print("Fault index avg: ", fault_index)
    # print(feature_set[:,0])
    # print(normal_fft[:,0])

    # Compare to threshold to see if we have an anomaly
    if fault_index < ANOMALY_THRESHOLD:
        print("ANOMALY DETECTED!")
    else:
        print("Normal")

    return

# Handler class for HTTP requests
class SimpleHTTPRequestHandler(BaseHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
    def do_GET(self):
        # Tell client if server is ready for a new sample
        self.send_response(200)
        self.end_headers()
        self.wfile.write(str(server_ready).encode())
    def do_POST(self):
        # Read message
        content_length = int(self.headers['Content-Length'])
        body = self.rfile.read(content_length)
        # Respond with 204 "no content" status code
        self.send_response(204)
        self.end_headers()
        # Decode JSON and compute MSE
        parseSamples(body.decode('ascii'))
# Server thread
class ServerThread(threading.Thread):
    def __init__(self, *args, **kwargs):
        super(ServerThread, self).__init__(*args, **kwargs)
        self._stop_event = threading.Event()
    def stop(self):
        self._stop_event.set()
    def is_stopped(self):
        return self._stop_event.is_set()
################################################################################
# Main
# Parse arguments
parser = argparse.ArgumentParser(description='Server that receives data from' +
                                    'IoT sensor node and detects anomalies.')
parser.add_argument('-p', action='store', dest='port', type=int,
                    default=DEFUALT_PORT, help='Port number for server')
args = parser.parse_args()
port = args.port


# Create server
handler = partial(SimpleHTTPRequestHandler)
server = HTTPServer(('', port), handler)
server_addr = server.socket.getsockname()
print('Server running at: ' + str(server_addr[0]) + ':' + 
        str(server_addr[1]))

# Create thread running server
server_thread = ServerThread(name='server_daemon',
                            target=server.serve_forever)
server_thread.daemon = True
server_thread.start()

# Store samples for given time
server_ready = 1
while True:
    pass
print('Server shutting down')
server.shutdown()
server_thread.stop()
server_thread.join()
