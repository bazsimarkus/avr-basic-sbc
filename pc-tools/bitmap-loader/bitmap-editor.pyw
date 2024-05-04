"""
AVR-SBC Binary Bitmap Image Editor
Author: Balazs Markus

This program allows you to convert any input image into an 128x64 binary bitmap image suitable for use with the AVR-SBC single board computer. The generated binary images can be loaded onto the AVR-SBC using the Bitmap Loader Script.

The functionality of the program is as follows:

1. Browse Image: Click the "Browse Image" button to select an image file from your computer. The selected image will be displayed in the "Input" section.

2. Image Previews: The selected input image will be displayed in the "Input" section. Any modifications made to the image will be displayed in the "Output" section in real-time.

3. Options:
   - Inverting: Check the "Invert Colors" checkbox to invert the colors of the image.
   - Dithering Type: Select a dithering type from the dropdown menu to apply dithering to the image.
   - Brightness Threshold: Adjust the brightness threshold slider to control the threshold for converting the image to binary.
   - Scaling: Choose a scaling option from the dropdown menu to resize the image.
   - Centering: Check the "Center Horizontally" and/or "Center Vertically" checkboxes to center the image within the canvas.
   - Background color: Select the background color for the canvas.
   - Rotate Image: Choose the rotation angle for the image.
   - Flip: Check the "Flip Horizontal" and/or "Flip Vertical" checkboxes to flip the image horizontally or vertically.

4. Save Bitmap: Click the "Save Bitmap" button to save the modified image as a 128x64 binary BMP file. You will be prompted to choose the save location and filename.

Usage:
1. Run the program.
2. Browse and select an image.
3. Adjust the options as desired.
4. Click "Save Bitmap" to generate the binary BMP image.
5. Use the separate Bitmap Loader Script that is next to this script to load the generated BMP file onto the AVR-SBC.
"""

import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image, ImageTk, ImageOps, ImageFilter, ImageDraw
import os
from os.path import exists

class ImageProcessor:
    """
    Class for managing image processing operations and GUI interactions.
    """
    def __init__(self, root):
        """
        Initialize the ImageProcessor class with the given root window.
        """
        self.root = root
        self.root.title("AVR-SBC Bitmap Editor v1.0")
        self.root.geometry("420x650")  # Set fixed size and make the window a little wider
        self.root.resizable(width=False, height=False)
        if exists("bitmap-loader-icon.ico"):
            self.root.iconbitmap("bitmap-loader-icon.ico")
            
        self.image = None
        self.modified_image = None

        self.invert_var = tk.BooleanVar()
        self.dither_var = tk.StringVar()
        self.dither_var.set("None")
        self.threshold_var = tk.DoubleVar(value=128)
        self.scale_var = tk.StringVar()
        self.scale_var.set("Stretch to fill canvas")
        self.center_horizontally_var = tk.BooleanVar()
        self.center_vertically_var = tk.BooleanVar()
        self.rotate_var = tk.IntVar()
        self.flip_horizontal_var = tk.BooleanVar()
        self.flip_vertical_var = tk.BooleanVar()
        self.bg_color_var = tk.StringVar()
        self.bg_color_var.set("Black")
        
        self.create_widgets()

        # Show "no image loaded" thumbnails on startup
        self.show_image()
    
    def create_widgets(self):
        """
        Create GUI widgets for image browsing, options selection, and image previews.
        """
        # Title Label
        title_label = tk.Label(self.root, text="AVR-SBC Bitmap Editor v1.0", font=("Helvetica", 12, "bold"))
        title_label.pack(pady=8)
        
        subtitle_label = tk.Label(self.root, text="Tool to generate 128x64 binary BMP images\nfor use with the AVR-SBC single board computer.\nThe generated binary images can be loaded onto the AVR-SBC\nusing the Bitmap Loader Script.", font=("Arial", 8))
        subtitle_label.pack(pady=0)
        
        # Browse Image Button and Entry Field
        browse_frame = tk.Frame(self.root)
        browse_frame.pack(pady=10)
        self.file_path_var = tk.StringVar()
        self.path_entry = tk.Entry(browse_frame, width=40, textvariable=self.file_path_var, state="readonly")
        self.path_entry.pack(side=tk.LEFT, padx=10)
        tk.Button(browse_frame, text="Browse Image", command=self.browse_image).pack(side=tk.LEFT)

        # Image Previews
        self.input_label = tk.Label(self.root, text="Input")
        self.input_label.pack()
        self.input_frame = tk.Frame(self.root, bd=1, relief=tk.SOLID, width=128, height=64)  # Add frame placeholder for input image
        self.input_frame.pack()
        self.input_thumbnail_label = tk.Label(self.input_frame)
        self.input_thumbnail_label.pack()

        self.output_label = tk.Label(self.root, text="Output")
        self.output_label.pack()
        self.output_frame = tk.Frame(self.root, bd=1, relief=tk.SOLID, width=128, height=64)  # Add frame placeholder for output image
        self.output_frame.pack()
        self.output_thumbnail_label = tk.Label(self.output_frame)
        self.output_thumbnail_label.pack()

        # Options
        options_frame = tk.Frame(self.root)
        options_frame.pack(pady=5)

        tk.Label(options_frame, text="Inverting:").grid(row=0, column=0, sticky=tk.W)
        tk.Checkbutton(options_frame, text="Invert Colors", variable=self.invert_var, command=self.update_output_thumbnail).grid(row=0, column=1, sticky=tk.W)

        tk.Label(options_frame, text="Dithering Type:").grid(row=1, column=0, sticky=tk.W)
        dithering_menu = tk.OptionMenu(options_frame, self.dither_var, "None", "Floyd-Steinberg", command=self.update_output_thumbnail)
        dithering_menu.config(width=17)  # Set fixed width
        dithering_menu.grid(row=1, column=1, sticky=tk.W)

        tk.Label(options_frame, text="Brightness Threshold:").grid(row=2, column=0, sticky=tk.W)
        tk.Scale(options_frame, from_=0, to=255, orient=tk.HORIZONTAL, variable=self.threshold_var, command=self.update_output_thumbnail).grid(row=2, column=1, sticky=tk.W)

        tk.Label(options_frame, text="Scaling:").grid(row=3, column=0, sticky=tk.W)
        scaling_menu = tk.OptionMenu(options_frame, self.scale_var, "None", "Scale to fit", "Stretch to fill canvas", command=self.update_output_thumbnail)
        scaling_menu.config(width=17)  # Set fixed width
        scaling_menu.grid(row=3, column=1, sticky=tk.W)

        tk.Label(options_frame, text="Centering:").grid(row=4, column=0, sticky=tk.W)
        tk.Checkbutton(options_frame, text="Center Horizontally", variable=self.center_horizontally_var, command=self.update_output_thumbnail).grid(row=4, column=1, sticky=tk.W)
        tk.Checkbutton(options_frame, text="Center Vertically", variable=self.center_vertically_var, command=self.update_output_thumbnail).grid(row=4, column=2, sticky=tk.W)

        tk.Label(options_frame, text="Background color:").grid(row=5, column=0, sticky=tk.W)
        black_radio = tk.Radiobutton(options_frame, text="Black", variable=self.bg_color_var, value="Black", command=self.update_output_thumbnail)
        black_radio.grid(row=5, column=1, sticky=tk.W)
        white_radio = tk.Radiobutton(options_frame, text="White", variable=self.bg_color_var, value="White", command=self.update_output_thumbnail)
        white_radio.grid(row=5, column=2, sticky=tk.W)

        tk.Label(options_frame, text="Rotate Image:").grid(row=6, column=0, sticky=tk.W)
        tk.Radiobutton(options_frame, text=str(0), variable=self.rotate_var, value=0, command=self.update_output_thumbnail).grid(row=6, column=1, sticky=tk.W)
        tk.Radiobutton(options_frame, text=str(90), variable=self.rotate_var, value=90, command=self.update_output_thumbnail).grid(row=6, column=2, sticky=tk.W)
        tk.Radiobutton(options_frame, text=str(180), variable=self.rotate_var, value=180, command=self.update_output_thumbnail).grid(row=7, column=1, sticky=tk.W)
        tk.Radiobutton(options_frame, text=str(270), variable=self.rotate_var, value=270, command=self.update_output_thumbnail).grid(row=7, column=2, sticky=tk.W)

        tk.Label(options_frame, text="Flip:").grid(row=8, column=0, sticky=tk.W)
        tk.Checkbutton(options_frame, text="Flip Horizontal", variable=self.flip_horizontal_var, command=self.update_output_thumbnail).grid(row=8, column=1, sticky=tk.W)
        tk.Checkbutton(options_frame, text="Flip Vertical", variable=self.flip_vertical_var, command=self.update_output_thumbnail).grid(row=8, column=2, sticky=tk.W)


        tk.Button(self.root, text="Save Bitmap", command=self.save_bitmap).pack(pady=5)  # Add space between buttons


    def browse_image(self):
        """
        Open a file dialog to browse and select an image file.
        """
        file_path = filedialog.askopenfilename()
        if file_path:
            self.image = Image.open(file_path)
            self.file_path_var.set(file_path)
            self.show_image()

    def show_image(self):
        """
        Display the selected input image in the GUI.
        """
        if self.image:
            self.modified_image = self.image.copy()
            self.update_output_thumbnail()
            input_photo = ImageTk.PhotoImage(self.image.resize((128, 64)))
            self.input_thumbnail_label.config(image=input_photo)
            self.input_thumbnail_label.image = input_photo
        else:
            # Display "no image loaded" thumbnails
            no_image = Image.new("RGB", (128, 64), "white")
            draw = ImageDraw.Draw(no_image)
            draw.text((22, 26), "No Image Loaded", fill="black")
            no_image_photo = ImageTk.PhotoImage(no_image)
            self.input_thumbnail_label.config(image=no_image_photo)
            self.input_thumbnail_label.image = no_image_photo

            self.output_thumbnail_label.config(image=no_image_photo)
            self.output_thumbnail_label.image = no_image_photo

    def update_output_thumbnail(self, *args):
        """
        Update the output thumbnail image based on the applied modifications.
        """
        if self.image:
            self.modified_image = self.image.copy()
            self.apply_modifications()
            output_photo = ImageTk.PhotoImage(self.modified_image.resize((128, 64)))
            self.output_thumbnail_label.config(image=output_photo)
            self.output_thumbnail_label.image = output_photo

    def apply_modifications(self):
        """
        Apply modifications to the image based on selected options.
        """
        if self.rotate_var.get():
            self.modified_image = self.modified_image.rotate(self.rotate_var.get(), expand=True)

        if self.flip_horizontal_var.get():
            self.modified_image = self.modified_image.transpose(Image.FLIP_LEFT_RIGHT)

        if self.flip_vertical_var.get():
            self.modified_image = self.modified_image.transpose(Image.FLIP_TOP_BOTTOM)

        # Apply scaling based on the selected option
        if self.scale_var.get() == "None":
            # No scaling, keep the original size
            target_width, target_height = self.modified_image.size
        else:
            # Determine scaling dimensions
            width, height = self.modified_image.size
            target_width, target_height = 128, 64

            if self.scale_var.get() == "Scale to fit":
                # Calculate scaling ratio
                width_ratio = target_width / width
                height_ratio = target_height / height
                # Choose the minimum ratio to fit the image within canvas
                scaling_ratio = min(width_ratio, height_ratio)
                # Calculate new dimensions
                target_width = int(width * scaling_ratio)
                target_height = int(height * scaling_ratio)

            elif self.scale_var.get() == "Stretch to fill canvas":
                # Resize the image to fit canvas exactly
                target_width, target_height = 128, 64

            # Resize the image
            self.modified_image = self.modified_image.resize((target_width, target_height), Image.LANCZOS)

        # Apply centering if necessary
        if self.center_horizontally_var.get() or self.center_vertically_var.get():
            bg_color = "black" if self.bg_color_var.get() == "Black" else "white"
            bg = Image.new("RGB", (128, 64), bg_color)
            x_offset = (128 - target_width) // 2 if self.center_horizontally_var.get() else 0
            y_offset = (64 - target_height) // 2 if self.center_vertically_var.get() else 0
            bg.paste(self.modified_image, (x_offset, y_offset))
            self.modified_image = bg
        else:
            # Place the image in the upper-left corner
            bg_color = "black" if self.bg_color_var.get() == "Black" else "white"
            bg = Image.new("RGB", (128, 64), bg_color)
            bg.paste(self.modified_image, (0, 0))
            self.modified_image = bg
            
         # Apply dithering
        dithering_type = self.dither_var.get()
        if dithering_type == "Floyd-Steinberg":
            self.modified_image = self.modified_image.convert("1", dither=Image.FLOYDSTEINBERG)
        else:
            if dithering_type == "None":
                # Convert to grayscale before thresholding
                self.modified_image = self.modified_image.convert("L")
                # Apply thresholding
                threshold = self.threshold_var.get()
                self.modified_image = self.modified_image.point(lambda x: 0 if x < threshold else 255)
            else:
                self.modified_image = self.modified_image.convert("L")

        # Invert the image if the checkbox is checked
        if self.invert_var.get():
            self.modified_image = ImageOps.invert(self.modified_image)

    def save_bitmap(self):
        """
        Save the modified image as a binary BMP file.
        """
        if not self.modified_image:
            messagebox.showerror("Error", "Please select an image first.")
            return

        # Convert to binary bitmap
        threshold = self.threshold_var.get()
        if self.modified_image.mode != 'L':
            self.modified_image = self.modified_image.convert('L')
        binary_image = self.modified_image.point(lambda x: 0 if x < threshold else 255, '1')

        # Save as BMP
        file_path = filedialog.asksaveasfilename(defaultextension=".bmp", filetypes=(("BMP files", "*.bmp"), ("All files", "*.*")))
        if file_path:
            binary_image.save(file_path)

if __name__ == "__main__":
    root = tk.Tk()
    app = ImageProcessor(root)
    root.mainloop()
