"""
MIDI to .bas Converter Application

This application allows users to convert single-track MIDI files into .BAS files. These .BAS files can be loaded onto the AVR-SBC single board computer using SBC Studio. The songs can then be played using the onboard buzzer speaker on the AVR-SBC.

Functionality:
1. MIDI to Text Conversion:
    - Convert MIDI files into text format compatible with AVR-SBC.
    - Convert MIDI note numbers to frequencies.
    - Handle note duration and silence between notes.

2. GUI Interface:
    - Browse: Select MIDI files using a file dialog.
    - Convert: Convert selected MIDI file to .BAS format.
    - Display conversion status.

Usage:
1. Run the application.
2. Click on the 'Browse' button to select a MIDI file.
3. Once the MIDI file is selected, the file path will be displayed in the entry field.
4. Click on the 'Convert' button to initiate the conversion process.
5. Upon successful conversion, a .BAS file will be created in the same directory as the original MIDI file.
6. Conversion status will be displayed in the application window.

Dependencies:
1. pretty_midi: Library for handling MIDI files.
2. tkinter: GUI toolkit for Python.

Example Usage:
1. Select a MIDI file using the 'Browse' button.
2. Click on 'Convert'.
3. Check the status message for conversion success or failure.
4. Locate the converted .BAS file in the same directory as the original MIDI file.

Note: These .BAS files can be loaded onto the AVR-SBC single board computer using SBC Studio. The songs can then be played using the onboard buzzer speaker on the AVR-SBC by typing RUN.
"""

import pretty_midi
import sys
import os
from os.path import exists
import tkinter as tk
from tkinter import filedialog

max_output_file_size_in_bytes = 7400

# Function to convert MIDI note number to frequency
def note_to_frequency(note):
    """
    Convert MIDI note number to frequency.

    Args:
    - note (int): MIDI note number.

    Returns:
    - float: Frequency corresponding to the MIDI note.
    """
    return round(440 * (2 ** ((note - 69) / 12)))

# Function to convert MIDI file to text format
def midi_to_text(midi_file):
    """
    Convert MIDI file to text format compatible with AVR-SBC.

    Args:
    - midi_file (str): Path to the MIDI file.

    Returns:
    - str: Text representation of the MIDI file.
    """
    try:
        midi_data = pretty_midi.PrettyMIDI(midi_file)

        tempo = 500000  # Default tempo (500000 microseconds per beat)

        output_lines = []

        line_number = 10
        previous_end_time = 0
        output_size = 0

        for instrument in midi_data.instruments:
            notes = sorted(instrument.notes, key=lambda x: x.start)
            for i in range(len(notes)):
                note = notes[i]
                note_freq = note_to_frequency(note.pitch)
                duration_ms = round((note.end - note.start) * 1000)
                
                # Check if there's another note that starts before this one ends
                next_note_start = notes[i+1].start if i < len(notes) - 1 else 1000000
                adjusted_duration_ms = min(duration_ms, round((next_note_start - note.start) * 1000))
                
                if note.start > previous_end_time:
                    # Calculate the duration of silence
                    silence_duration_ms = round((note.start - previous_end_time) * 1000)
                    output_line = f"{line_number} DELAY {silence_duration_ms}"
                    if output_size + len(output_line) + 2 > max_output_file_size_in_bytes:
                        return '\n'.join(output_lines)
                    output_lines.append(output_line)
                    output_size += len(output_line) + 2 # We have to add 2 because of CR-LF
                    line_number += 10
                    print(output_line)
                
                output_line = f"{line_number} TONEW {note_freq},{adjusted_duration_ms}"
                if output_size + len(output_line) + 2 > max_output_file_size_in_bytes:
                    return '\n'.join(output_lines)
                output_lines.append(output_line)
                output_size += len(output_line) + 2 # We have to add 2 because of CR-LF
                line_number += 10
                previous_end_time = note.end
                print(output_line)
            break

        return '\n'.join(output_lines)

    except Exception as e:
        print(f"Error: {e}")
        return None

def browse_file():
    """
    Open file dialog to browse MIDI files.
    """
    filename = filedialog.askopenfilename(filetypes=[("MIDI files", "*.mid;*.midi")])
    if filename:
        entry_path.delete(0, tk.END)
        entry_path.insert(0, filename)

def convert_midi():
    """
    Convert selected MIDI file to .BAS format.
    """
    midi_file = entry_path.get()
    if midi_file:
        output_text = midi_to_text(midi_file)
        if output_text:
            output_filename = os.path.splitext(midi_file)[0] + '.bas'
            with open(output_filename, 'w') as output_file:
                output_file.write(output_text)
            label_status.config(text=f"Conversion successful.\n Output saved to {output_filename}")
        else:
            label_status.config(text="Conversion failed. Please check the MIDI file.")
    else:
        label_status.config(text="Please select a MIDI file.")

# GUI setup
root = tk.Tk()
root.title("MIDI to .bas Converter")
root.resizable(width=False, height=False)

if exists("midi-to-bas-converter-icon.ico"):
    root.iconbitmap("midi-to-bas-converter-icon.ico")

frame = tk.Frame(root)
frame.pack(padx=10, pady=10)

label_title = tk.Label(frame, text="MIDI to .bas Converter", font=("Arial", 14, "bold"))
label_title.grid(row=0, column=0, columnspan=3, pady=(0, 10))

label_description = tk.Label(frame, text="With this app, you can convert single-track MIDI files to .BAS files \nthat can be loaded onto the AVR-SBC single board computer using SBC Studio. \nThe songs can then be played using the onboard buzzer speaker on the AVR-SBC.")
label_description.grid(row=1, column=0, columnspan=3, pady=(0, 10))

label_note = tk.Label(frame, text="🎵")
label_note.grid(row=2, column=0, padx=5)

button_browse = tk.Button(frame, text="Browse", command=browse_file)
button_browse.grid(row=2, column=1, padx=5, pady=5)

entry_path = tk.Entry(frame, width=40)
entry_path.grid(row=2, column=2, padx=5, pady=5)

button_convert = tk.Button(frame, text="Convert", command=convert_midi)
button_convert.grid(row=3, column=1, columnspan=2, padx=5, pady=5)

label_status = tk.Label(frame, text="")
label_status.grid(row=4, column=0, columnspan=3, pady=5)

root.mainloop()
