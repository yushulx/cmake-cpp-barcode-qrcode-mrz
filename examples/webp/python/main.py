#!/usr/bin/env python3

'''
Usage:
   main.py [image-file] [optional: license-file]
'''

from dbr import *
import cv2


def main():
    try:
        filename = sys.argv[1]
        license = "LICENSE-KEY"

        if len(sys.argv) > 2:
            with open(sys.argv[2]) as f:
                license = f.read()

        frame = cv2.imread(filename)
        reader = BarcodeReader()
        ret = reader.init_license(license)
        print('License status {0}'.format(ret))

        results = reader.decode_buffer(frame)
        index = 0
        for result in results:
            points = result.localization_result.localization_points
            print("Index: " + str(index) + "\n")
            print("     Barcode format: " + result.barcode_format_string + '\n')
            print("     Barcode value: " + result.barcode_text + '\n')
            print("     Points: " + str(points[0]) + ' ' + str(
                points[1]) + ' ' + str(points[2]) + ' ' + str(points[3]) + '\n')
            print('-----------------------------------\n')
            index += 1

    except:
        print(__doc__)


if __name__ == '__main__':
    main()
