import struct
import ctypes
import sys
import os

RT_ICON = 3
RT_GROUP_ICON = 14

def embed_icon(exe_path, ico_path):
    with open(ico_path, 'rb') as f:
        ico_data = f.read()

    # Parse ICONDIR
    reserved, ico_type, count = struct.unpack('<HHH', ico_data[:6])
    if ico_type != 1:
        raise ValueError("Not a valid .ico file")

    entries = []
    offset = 6
    for i in range(count):
        width, height, color_count, rsvd, planes, bit_count, bytes_in_res, image_offset = struct.unpack(
            '<BBBBHHII', ico_data[offset:offset+16]
        )
        entries.append({
            'id': i + 1,
            'width': width,
            'height': height,
            'color_count': color_count,
            'reserved': rsvd,
            'planes': planes,
            'bit_count': bit_count,
            'bytes_in_res': bytes_in_res,
            'image_offset': image_offset,
            'data': ico_data[image_offset:image_offset + bytes_in_res]
        })
        offset += 16

    # Build GRPICONDIR
    grp_dir = struct.pack('<HHH', reserved, ico_type, count)
    for entry in entries:
        grp_dir += struct.pack(
            '<BBBBHHIH',
            entry['width'], entry['height'], entry['color_count'], entry['reserved'],
            entry['planes'], entry['bit_count'], entry['bytes_in_res'], entry['id']
        )

    k32 = ctypes.windll.kernel32
    k32.BeginUpdateResourceW.argtypes = [ctypes.c_wchar_p, ctypes.c_bool]
    k32.BeginUpdateResourceW.restype = ctypes.c_void_p
    k32.EndUpdateResourceW.argtypes = [ctypes.c_void_p, ctypes.c_bool]
    k32.EndUpdateResourceW.restype = ctypes.c_bool
    k32.UpdateResourceW.argtypes = [
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_ushort,
        ctypes.c_char_p,
        ctypes.c_ulong
    ]
    k32.UpdateResourceW.restype = ctypes.c_bool

    h_update = k32.BeginUpdateResourceW(exe_path, False)
    if not h_update:
        err = k32.GetLastError()
        raise RuntimeError(f"BeginUpdateResource failed: {err}")

    # Add each RT_ICON
    for entry in entries:
        data = entry['data']
        res = k32.UpdateResourceW(
            h_update,
            ctypes.c_void_p(RT_ICON),
            ctypes.c_void_p(entry['id']),
            0, # neutral lang
            data,
            len(data)
        )
        if not res:
            k32.EndUpdateResourceW(h_update, True)
            raise RuntimeError(f"UpdateResource RT_ICON {entry['id']} failed: {k32.GetLastError()}")

    # Add RT_GROUP_ICON (ID 1)
    res = k32.UpdateResourceW(
        h_update,
        ctypes.c_void_p(RT_GROUP_ICON),
        ctypes.c_void_p(1),
        0,
        grp_dir,
        len(grp_dir)
    )
    if not res:
        k32.EndUpdateResourceW(h_update, True)
        raise RuntimeError(f"UpdateResource RT_GROUP_ICON failed: {k32.GetLastError()}")

    if not k32.EndUpdateResourceW(h_update, False):
        raise RuntimeError(f"EndUpdateResource failed: {k32.GetLastError()}")

    print(f"Successfully embedded {count} icon images into {exe_path}!")

if __name__ == '__main__':
    exe = sys.argv[1]
    ico = sys.argv[2]
    embed_icon(exe, ico)
