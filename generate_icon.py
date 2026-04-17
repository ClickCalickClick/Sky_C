from PIL import Image, ImageDraw
import sys

def create_icon():
    img = Image.new('RGB', (25, 25), color=(0, 170, 255))
    d = ImageDraw.Draw(img)
    d.ellipse([(5,5), (19,19)], fill=(255, 255, 255))
    img.save('resources/images/menu_icon.png')

if __name__ == '__main__':
    create_icon()
