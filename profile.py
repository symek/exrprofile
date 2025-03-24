import OpenEXR
import Imath
import numpy as np
import time
import os
import argparse

def generate_test_image(width, height):
    """Generate a simple gradient image with noise."""
    print(f"\n=== Generating data {width}x{height} ===")
    data = np.linspace(0, 1, width * height, dtype=np.float32)
    # Add approximately 15% noise to each value
    noise = np.random.normal(0, 0.15, width * height).astype(np.float32)
    # From now on 16bit float
    noisy_data = np.clip(data + noise, 0.0, 1.0).astype(np.float16)
    return noisy_data.tobytes()

def compress_and_write_image(filename, compression_type, data, width, height):
    """Compress and write an image using the specified OpenEXR compression."""
    header = OpenEXR.Header(width, height)
    header['channels'] = {
        'R': Imath.Channel(Imath.PixelType(Imath.PixelType.HALF)),
        'G': Imath.Channel(Imath.PixelType(Imath.PixelType.HALF)),
        'B': Imath.Channel(Imath.PixelType(Imath.PixelType.HALF))
    }
    # Set the correct compression type
    header['compression'] = Imath.Compression(compression_type)
    # print(f"=== Writing to file {filename} ===")
    start_time = time.time()
    exr = OpenEXR.OutputFile(filename, header)
    exr.writePixels({'R': data, 'G': data, 'B': data})
    exr.close()
    end_time = time.time()

    return end_time - start_time

def load_and_decompress_image(filename):
    """Load and decompress an image."""
    # print(f"=== Reading from file {filename} ===")
    start_time = time.time()
    exr = OpenEXR.InputFile(filename)
    channels = exr.header()['channels'].keys()
    data = {channel: exr.channel(channel, Imath.PixelType(Imath.PixelType.FLOAT)) for channel in channels}
    exr.close()
    end_time = time.time()

    return end_time - start_time

def profile_compression(compression_type, data, width, height,  cleanup=False):
    filename = f"test_{compression_type}.exr"
    compression_enum = getattr(Imath.Compression, compression_type)
    # Compression and Writing
    compression_time = compress_and_write_image(filename, compression_enum, data, width, height)

    # Loading and Decompression
    decompression_time = load_and_decompress_image(filename)

    # Cleanup
    if cleanup and os.path.exists(filename):
        os.remove(filename)

    return compression_time, decompression_time


def main():

    opts = argparse.ArgumentParser(description="Checkout whats up with OpenEXR compressions.")

    # Add arguments
    opts.add_argument("-s", "--scale", type=int, default=4, help="Multiply of 1024x1024 pixel of size (default 4)")
    opts.add_argument("-c", "--clean", action="store_true", help="Clean exr files after finishing test.")

    # Parse the arguments
    args = opts.parse_args()

    # Image dimensions
    mult = args.scale
    width, height = 1024*mult, 1024*mult  # Higher resolution

    # Compression to be tested
    compressions = ['B44A_COMPRESSION', 'B44_COMPRESSION', 'DWAA_COMPRESSION', 
                    'DWAB_COMPRESSION', 'NO_COMPRESSION', 'PIZ_COMPRESSION', 'PXR24_COMPRESSION', 
                    'RLE_COMPRESSION', 'ZIPS_COMPRESSION', 'ZIP_COMPRESSION']


    # Generate test image data with noise
    data = generate_test_image(width, height)

    results = dict()

    print("\n=== Profiling Compression ===\n")
    for comp in compressions:
        time_compress, time_decompress = profile_compression(comp, data, width, height, args.clean)
        print(f"{comp:>17} compress & writing: {time_compress:.6f} seconds")
        print(f"{comp:>17} read  & decompress: {time_decompress:.6f} seconds")
        results[comp] = (time_compress, time_decompress)



    sorted_by_compress = dict(sorted(results.items(), key=lambda x: x[1][0]))
    print("\n=== Compression winners ===\n")
    for name, value in sorted_by_compress.items():
        value = value[0]
        print(f"{name:>17}: {value:.6f} seconds")

    sorted_by_decomp = dict(sorted(results.items(), key=lambda x: x[1][1]))
    print("\n=== Decompression winners ===\n")
    for name, value in sorted_by_decomp.items():
        value = value[1]
        print(f"{name:>17}: {value:.6f} seconds")

if __name__ == "__main__": main()

