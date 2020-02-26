from scipy.io import wavfile
import numpy as np
import git

# Get the git root.
repo = git.Repo('.', search_parent_directories=True)

# samplerate, data = wavfile.read(repo.working_tree_dir + '/resources/train_short.wav')
samplerate = 44100

file_txt = repo.working_tree_dir + '/resources/new_data_array.txt'
f = open(file_txt, "r")
data_write = f.read().split(',')
data_write = np.asarray(data_write, dtype=np.int16)
print(data_write)

file_wav = repo.working_tree_dir + '/resources/new_data_array.wav'
wavfile.write(file_wav,
        samplerate, data_write)
f.close()