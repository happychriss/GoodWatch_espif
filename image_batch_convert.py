import os
from PIL import Image

# Constants for source and destination folders
SRC_FOLDER = './supporting_material/weather_icons/used_icons_set_png'
DEST_FOLDER = './data'
SIZE = 90

def convert_images(src_folder, dest_folder):
    # Iterate through all files in the source folder
    for filename in os.listdir(src_folder):
        if filename.endswith(".png"):
            # Construct full file path
            filepath = os.path.join(src_folder, filename)

            # Open the image
            with Image.open(filepath) as im:

                im=im.resize((SIZE, SIZE))
                bg = Image.new("RGB", im.size, (255, 255, 255))
                bg.paste(im, im)
                im = bg

                # Convert to grayscale
                grayscale_img = im.convert("L")

                # Apply binary thresholding
                binary_img = grayscale_img.point(lambda x: 0 if x < 128 else 255, '1')

                # Create destination path
                dest_filepath = os.path.join(dest_folder, os.path.splitext(filename)[0] + '.bmp')

                # Save as BMP
                binary_img.save(dest_filepath, "BMP")

if __name__ == "__main__":
    # Convert images
    convert_images(SRC_FOLDER, DEST_FOLDER)
