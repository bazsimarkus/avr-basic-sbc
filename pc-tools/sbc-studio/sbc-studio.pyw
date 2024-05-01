"""
SBC Studio v1.0
IDE for programming AVR-SBC single board computers

Author: Balazs Markus (github.com/bazsimarkus)
License: MIT
Copyright: 2024 bazsimarkus
"""

import os
from os.path import exists
import tkinter as tk
from tkinter import ttk
from chlorophyll import CodeView
import pygments.lexers
from tkinter import filedialog
from tkinter import messagebox
import serial
import serial.tools.list_ports


# Function to list available COM ports
def list_com_ports():
    com_ports = serial.tools.list_ports.comports()
    return [port.device for port in com_ports]

# Function to create a submenu with available COM ports
def create_com_port_submenu():
    com_port_menu.delete(0, 'end')
    com_port_menu.add_radiobutton(label="None", variable=selected_port, value="None")
    for port in list_com_ports():
        com_port_menu.add_radiobutton(label=port, variable=selected_port, value=port)
    if selected_port.get() not in list_com_ports():
        selected_port.set("None")
    program_menu.entryconfig(0, label="Select COM port (" + selected_port.get() + ")")

# Function to start programming
def start_programming():
    selected_com_port = selected_port.get()
    available_com_ports = list_com_ports()

    if selected_com_port == "None":
        messagebox.showerror("Error", "Please select a COM port.")
    elif selected_com_port not in available_com_ports:
        messagebox.showwarning("Warning", "Selected COM port is not available. Resetting to 'None'.")
        selected_port.set("None")
    else:
        result = messagebox.askokcancel("Start Programming", "Type \"SERLOAD\" on the AVR-SBC to put the device in programming mode.\nYou can open the serial port with \"SEROPEN\".\n\nStart programming?")
        if result:
            # Get the content of the currently opened tab
            current_tab = notebook.select()
            current_code_view = notebook.nametowidget(current_tab).winfo_children()[0].winfo_children()[0]
            content = current_code_view.get("1.0", "end-1c")

            # Send the content to the selected COM port with baud rate 9600
            try:
                ser = serial.Serial(selected_com_port, baudrate=9600, timeout=5)
                print(content.encode())
                ser.write(content.encode())
                ser.write(b'\x00')  # Terminating zero
                ser.close()
                messagebox.showinfo("Success", "Programming successful.\n\nYou can run the loaded program by typing \"RUN\" on the AVR-SBC.")
            except serial.SerialException as e:
                messagebox.showerror("Error", f"Failed to send data to COM port: {e}")

def new_file():
    i = 1
    while any(notebook.tab(tab_id, "text") == f"Untitled {i}" for tab_id in notebook.tabs()):
        i += 1
    new_tab = ttk.Frame(notebook)
    notebook.add(new_tab, text=f"Untitled {i}")
    new_code_view = CodeView(new_tab, lexer=pygments.lexers.QBasicLexer, color_scheme="monokai")
    new_code_view.pack(side='top', fill="both", expand=True)
    new_code_view['undo'] = True
    new_code_view.bind("<Button-3>", show_popup) # Bind the popup to the right-click event
    notebook.select(new_tab)

def open_file():
    filepath = filedialog.askopenfilename(filetypes=[("BAS files", "*.bas")])
    if filepath:
        with open(filepath, 'r') as file:
            content = file.read()
        new_tab = ttk.Frame(notebook)
        notebook.add(new_tab, text=os.path.basename(filepath))
        new_code_view = CodeView(new_tab, lexer=pygments.lexers.QBasicLexer, color_scheme="monokai")
        new_code_view.pack(side='top', fill="both", expand=True)
        new_code_view.delete('1.0', 'end')  # Clear any existing content
        new_code_view.insert('1.0', content)  # Insert the content from the file
        new_code_view['undo'] = True
        new_code_view.bind("<Button-3>", show_popup) # Bind the popup to the right-click event
        notebook.select(new_tab)
        
def save_file(event=None):
    current_tab = notebook.select()
    current_code_view = notebook.nametowidget(current_tab).winfo_children()[0].winfo_children()[0]
    filename = notebook.tab(current_tab, "text")
    filepath = filedialog.asksaveasfilename(initialfile=filename, defaultextension=".bas", filetypes=[("BAS files", "*.bas")])
    if filepath:
        with open(filepath, 'w') as file:
            file.write(current_code_view.get("1.0", "end-1c"))  # Retrieve content from CodeView widget
        notebook.tab(current_tab, text=os.path.basename(filepath))

def exit_app():
    if messagebox.askokcancel("Quit", "Do you want to quit?"):
        root.destroy()

def show_help():
    help_text = """
    Welcome to SBC Studio v1.0 Help

    File Menu:
    - New: Create a new, empty file.
    - Open: Open an existing BAS file.
    - Save: Save the current file.
    - Exit: Close the application.

    Edit Menu:
    - Undo: Undo the last action.
    - Redo: Redo the last undone action.
    - Cut: Cut the selected text.
    - Copy: Copy the selected text.
    - Paste: Paste the copied/cut text.
    - Select All: Select all text in the editor.
    - Delete: Delete the selected text.

    Program Menu:
    - Select COM port: Choose the COM port for programming.
    - Start programming: Start programming the AVR-SBC.

    Help Menu:
    - Help: Display this help message.
    - About: Information about SBC Studio.

    Additional Shortcuts:
    - Control+S: Save the current file.
    - You can close a tab by right clicking its title and selecting "Close tab".

    Please note that some functionalities require a connected AVR-SBC device.
    """
    messagebox.showinfo("Help", help_text)

def show_about():
    messagebox.showinfo("About", "SBC Studio v1.0\nIDE for programming AVR-SBC single board computers\n\nAuthor: Balazs Markus (github.com/bazsimarkus)\nLicense: MIT\nCopyright: 2024 bazsimarkus")

def close_current_tab(event):
    tab_index = notebook.index('@%d,%d' % (event.x, event.y))
    notebook.forget(tab_index)

def show_context_menu(event):
    if len(notebook.tabs()) > 1:
        context_menu.entryconfig("Close tab", state="normal")
    else:
        context_menu.entryconfig("Close tab", state="disabled")
    context_menu.event = event
    context_menu.tk_popup(event.x_root, event.y_root)

def show_popup(event):
    popup.tk_popup(event.x_root, event.y_root)

def undo():
    current_tab = notebook.select()
    current_code_view = notebook.nametowidget(current_tab).winfo_children()[0].winfo_children()[0]
    current_code_view.event_generate("<<Undo>>")

def redo():
    current_tab = notebook.select()
    current_code_view = notebook.nametowidget(current_tab).winfo_children()[0].winfo_children()[0]
    current_code_view.event_generate("<<Redo>>")

def cut():
    current_tab = notebook.select()
    current_code_view = notebook.nametowidget(current_tab).winfo_children()[0].winfo_children()[0]
    current_code_view.event_generate("<<Cut>>")

def copy():
    current_tab = notebook.select()
    current_code_view = notebook.nametowidget(current_tab).winfo_children()[0].winfo_children()[0]
    current_code_view.event_generate("<<Copy>>")

def paste():
    current_tab = notebook.select()
    current_code_view = notebook.nametowidget(current_tab).winfo_children()[0].winfo_children()[0]
    current_code_view.event_generate("<<Paste>>")

def select_all():
    current_tab = notebook.select()
    current_code_view = notebook.nametowidget(current_tab).winfo_children()[0].winfo_children()[0]
    current_code_view.event_generate("<<SelectAll>>")

def delete():
    current_tab = notebook.select()
    current_code_view = notebook.nametowidget(current_tab).winfo_children()[0].winfo_children()[0]
    current_code_view.event_generate("<<Clear>>")


root = tk.Tk()

if exists("sbc-studio-icon.ico"):
    root.iconbitmap("sbc-studio-icon.ico")

root.title("SBC Studio v1.0")
root.option_add("*tearOff", 0)

menubar = tk.Menu(root)

file_menu = tk.Menu(menubar)
edit_menu = tk.Menu(menubar)
program_menu = tk.Menu(menubar)
help_menu = tk.Menu(menubar)

context_menu = tk.Menu(root, tearoff=0)
context_menu.add_command(label="Close tab", command=lambda: close_current_tab(context_menu.event))

notebook = ttk.Notebook(root)
notebook.bind("<Button-3>", show_context_menu)  # Button-3 is the right-click event

tab_1 = ttk.Frame(notebook)
notebook.add(tab_1, text="Untitled 1")
notebook.pack(fill="both", expand=True)

code_view = CodeView(tab_1, lexer=pygments.lexers.QBasicLexer, color_scheme="monokai")  # Changed parent to tab_1

# Create a new Menu
popup = tk.Menu(root, tearoff=0)
popup.add_command(label="Cut", command=cut)
popup.add_command(label="Copy", command=copy)
popup.add_command(label="Paste", command=paste)
popup.add_separator()
popup.add_command(label="Select All", command=select_all)
popup.add_command(label="Delete", command=delete)

# Bind the popup to the right-click event
code_view.bind("<Button-3>", show_popup)

code_view.pack(side='top', fill="both", expand=True)

#code_view.insert('1.0', "10 PRINT \"Hello World!\"\n20 END\n")
code_view['undo'] = True

file_menu.add_command(label="New", command=new_file)
file_menu.add_command(label="Open", command=open_file)
file_menu.add_command(label="Save", command=save_file)
file_menu.add_separator()
file_menu.add_command(label="Exit", command=exit_app)

edit_menu.add_command(label="Undo", command=undo)
edit_menu.add_command(label="Redo", command=redo)
edit_menu.add_separator()
edit_menu.add_command(label="Cut", command=cut)
edit_menu.add_command(label="Copy", command=copy)
edit_menu.add_command(label="Paste", command=paste)
edit_menu.add_separator()
edit_menu.add_command(label="Select All", command=select_all)
edit_menu.add_command(label="Delete", command=delete)

# Global variable to store the selected COM port
selected_port = tk.StringVar(value="None")

# Create a new menu for COM port selection
com_port_menu = tk.Menu(program_menu,postcommand=create_com_port_submenu)

program_menu.add_cascade(label="Select COM port (None)", menu=com_port_menu)
program_menu.add_separator()
program_menu.add_command(label="Start programming", command=start_programming)
# Bind the <<MenuSelect>> event to the create_com_port_submenu function
program_menu.bind("<<Button-1>>", create_com_port_submenu)

help_menu.add_command(label="Help", command=show_help)
help_menu.add_command(label="About", command=show_about)

menubar.add_cascade(menu=file_menu, label="File")
menubar.add_cascade(menu=edit_menu, label="Edit")
menubar.add_cascade(menu=program_menu, label="Program")
menubar.add_cascade(menu=help_menu, label="Help")

root.config(menu=menubar)

# Bind the save function to Control+S
root.bind_all("<Control-s>", save_file)

root.update()
root.minsize(root.winfo_width(), root.winfo_height())
root.mainloop()
