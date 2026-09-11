"""Read-only format-24/25/26/27/28 LM archive inspection; CRC checks are NOT SipHash auth.

Only saved facts are available. A comparison cannot certify cross-boot loading,
reconstruct uncaptured SYS allocator nodes, or establish matching live hardware.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib

try:
    from .lm_quick_codec import decode_fast
except ImportError:
    from lm_quick_codec import decode_fast

FORMAT = 24
SUPPORTED_FORMATS = (24, 25, 26, 27, 28)
PAYLOAD_MAX = 0xFD0000
SHARED_LIMIT = PAYLOAD_MAX - 0x2000
SHARED_SIZE = 0x419B00
STATICS_OFFSET, CAMERA_OFFSET, GAME_OFFSET = 0x148, 0x186D4, 0x189E0
TRAILER_SIZE = 0x1900
PROFILE_SIZE, PROFILE_MAGIC, PROFILE_RECORDS = 1440, 0x4C4D5052, 48
MEM1_START, MEM1_END = 0x80000000, 0x81800000
ENVELOPE_FIELDS = "magic version headerSize payloadSize gameId snapshotVersion buildCrc session rawSize trailerSize payloadCrc generation authHigh authLow reserved0 reserved1".split()
HEADER_FIELDS = ("magic version headerSize gameId totalSize checksum generation heap heapStart heapEnd heapSize "
    "heapMetadataOffset heapMetadataSize stateStaticsOffset stateStaticsSize heapDataOffset heapDataSize "
    "rootHeap rootHeapStart rootHeapEnd rootHeapSize rootHeapMode rootHeapGroup rootFreeHead rootFreeTail rootUsedHead rootUsedTail "
    "systemHeap systemHeapStart systemHeapEnd systemHeapSize currentHeap missionMode mapArchive volumeHead volumeTail volumeCount "
    "mapValue sceneValue currentScene gameMode gameModeCount heapMode heapGroup systemHeapMode systemHeapGroup "
    "systemFreeHead systemFreeTail systemUsedHead systemUsedTail currentHeapGroup randomState freeHead freeTail usedHead usedTail "
    "mainLoopMode mainLoopPendingScene mainDrawState simpleModeler mapCol enTypesManager audioBasic audioScene").split()
SHARED_FIELDS = "owner base size systemHeap systemStart systemEnd parentBlockTag ownerBlockTag usedSignature freeSignature".split()
PROFILE_FIELDS = "magic version generation checksum configId rootHeap systemHeap count".split()
PROFILE_ALLOCATION_FIELDS = "heap node tag bytes previous next".split()
PROFILE_ANCHORS = (
    "fifo.buffer", "fifo.object", "framebuffer.primary", "framebuffer.secondary",
    "framebuffer.videoPrimary", "framebuffer.videoSecondary", "renderMode.object", "pad.object",
    "fifo.base", "fifo.end", "fifo.bytes", "pad.vtable", "pad.heap", "pad.port",
    "renderMode.viTVMode", "renderMode.fbWidth_efbHeight", "renderMode.xfbHeight_viXOrigin",
    "renderMode.viYOrigin_viWidth", "renderMode.viHeight_padding", "renderMode.xfbMode",
    "renderMode.fieldRendering_aa_sample0",
    *(f"renderMode.samplePattern.word{i}" for i in range(1, 6)),
    "renderMode.samplePattern_vfilter", "renderMode.vfilter.word1", "renderMode.vfilter_padding",
    *(f"aram.{i}.{field}" for i in range(5) for field in ("address", "bytes")),
    "audio.data", *(f"audio.pool{i}.{field}" for i in range(3) for field in ("pointer", "count")),
    "audio.cameraMatrix", "audio.cameraUnused", "audio.cameraProjection")

# Frozen format-24 capture order. Changing a runtime range requires a new schema.
STATIC_RANGES = (
    ("transitionHeader", 0x803985D4, 0x14), ("transitionTail", 0x80398764, 0xC),
    ("renderer", 0x80398770, 0x270), ("cameraDescriptor", 0x80398BF8, 0x58),
    ("doorVisibility", 0x80399510, 0x620), ("cameraManager", 0x80399B60, 0x100),
    ("modelTable", 0x803435AC, 0x3538), ("modelRegistry", 0x8037EC70, 0x4180),
    ("resourceState", 0x80398C50, 0x378), ("roomEvents", 0x803C7CA0, 0x788),
    ("roomActors", 0x803C8490, 0x200), ("roomPropPictures", 0x803C1C60, 0x38),
    ("animatedModelOwners", 0x803C26C8, 0x6CC), ("roomVisibility", 0x803C2E10, 0x220),
    ("eventActive", 0x803C20C8, 0x70), ("roomName", 0x803C4628, 0xF0),
    ("gbhHud", 0x803C3238, 0x150), ("hudPictures", 0x803C3400, 0x330),
    ("dialogue", 0x803C3730, 0xD18), ("timerHud", 0x803C4448, 0x1E0),
    ("elementHud", 0x803C4718, 0x150), ("booRadar", 0x803C49C0, 0x18),
    ("modelRenderContext", 0x803C4A10, 0x40), ("depthTexture", 0x803C4B6C, 0x114),
    ("vrScene", 0x803C24E8, 0x1E0), ("modelOutput", 0x803C86A0, 0x1124),
    ("grainManagers", 0x803CBAF0, 0x970), ("effectModel0", 0x803CC9A4, 0x2A8),
    ("effectModel1", 0x803CCC58, 0x2A8), ("effectModel2", 0x803CCF0C, 0x2A8),
    ("lazyEffects", 0x803CC46C, 0x2AC), ("playerQuery", 0x803CC718, 0x100),
    ("sceneEffects", 0x803CD1F4, 0x2D4), ("particles", 0x803CD4FC, 0xBF4),
    ("effectControllers", 0x803CE0F0, 0xA10), ("modelRegistryOutput", 0x803E3088, 0xC70),
    ("mainLoop", 0x80398A40, 8), ("sdata0", 0x80498AF8, 0x20),
    ("sdata1", 0x80498B20, 0x7888), ("sbss0", 0x804A0C00, 0x90),
    ("sbss1", 0x804A0CB0, 0x1060), ("volumeList", 0x80494754, 12),
    ("currentVolume", 0x804A2038, 4), ("currentDirectory", 0x804A2040, 4))


def schema(version):
    """Keep old captures readable without interpreting them as the new layout."""
    if version in (27, 28):
        ranges = STATIC_RANGES[:-3] + (("roomInfo", 0x803C2138, 0x234),
            ("furnitureInfo", 0x803C236C, 0xFC),
            ("roomMapLookup", 0x803C2468, 0x80),
            ("roomMapUi", 0x803C1C98, 0x430)) + STATIC_RANGES[-3:]
        return ranges, 0x18EB4, 0x191C0
    if version == 26:
        ranges = STATIC_RANGES[:-3] + (("roomInfo", 0x803C2138, 0x234),) + STATIC_RANGES[-3:]
        return ranges, 0x18908, 0x18C20
    if version in (24, 25):
        return STATIC_RANGES, CAMERA_OFFSET, GAME_OFFSET
    raise ArchiveError("unsupported snapshot schema")


class ArchiveError(ValueError):
    pass


def require(condition: bool, message: str):
    if not condition:
        raise ArchiveError(message)


def words(data, offset: int, count: int):
    require(0 <= offset <= len(data) and count <= (len(data) - offset) // 4,
            "truncated word record")
    return struct.unpack_from(f">{count}I", data, offset)


def crc_zeroed(data, gaps):
    crc, offset = 0, 0
    for start, end in gaps:
        crc = zlib.crc32(data[offset:start], crc)
        crc = zlib.crc32(bytes(end - start), crc)
        offset = end
    return zlib.crc32(data[offset:], crc)


def inside(address, size, start, end):
    return size > 0 and start <= address < end and size <= end - address


def decode_parent(packed: bytes, quick_allowed=False):
    if quick_allowed and packed[:4] == b"LML4":
        try:
            return decode_fast(packed, SHARED_SIZE)
        except ValueError as error:
            raise ArchiveError(f"invalid companion LML4 stream: {error}") from error
    decoder = zlib.decompressobj()
    try:
        output = decoder.decompress(packed, SHARED_SIZE + 1)
    except zlib.error as error:
        raise ArchiveError(f"invalid companion zlib stream: {error}") from error
    require(len(output) == SHARED_SIZE and decoder.eof and not decoder.unconsumed_tail
            and not decoder.unused_data, "companion decoded size, end marker or trailing bytes invalid")
    return output


@dataclass
class Archive:
    path: str
    sha256: str
    envelope: dict
    header: dict
    shared: dict
    facts: dict
    warnings: list[str]
    profile: dict | None = None


def decode_profile(data, outer, h):
    """Validate saved wire shape only; uncaptured live services cannot be proven."""
    require(len(data) == PROFILE_SIZE, "reboot profile size invalid")
    portable = outer["version"] == 2
    if not any(data):
        require(not portable, "v2 archive requires a reboot profile")
        return None
    profile = dict(zip(PROFILE_FIELDS, words(data, 0, 8)))
    require((profile["magic"], profile["version"]) == (PROFILE_MAGIC, 1), "reboot profile magic/version invalid")
    require(profile["generation"] == h["generation"], "reboot profile generation mismatch")
    require(crc_zeroed(data, ((12, 16),)) == profile["checksum"], "reboot profile CRC mismatch")
    require(0 < profile["count"] <= PROFILE_RECORDS, "reboot profile allocation count invalid")
    require(profile["rootHeap"] == h["rootHeap"] and profile["systemHeap"] == h["systemHeap"],
            "reboot profile heap identity mismatch")
    require(profile["configId"] != 0 and (not portable or profile["configId"] == outer["reserved1"]),
            "reboot profile config mismatch")
    anchors = words(data, 32, 64)
    require(len(PROFILE_ANCHORS) == 49 and not any(anchors[len(PROFILE_ANCHORS):]),
            "reboot profile reserved anchors nonzero")
    profile["anchors"] = dict(zip(PROFILE_ANCHORS, anchors))
    allocations = [dict(zip(PROFILE_ALLOCATION_FIELDS, words(data, 288 + i * 24, 6)))
                   for i in range(profile["count"])]
    require(not any(data[288 + profile["count"] * 24:]), "reboot profile unused allocations nonzero")
    require(all(a["heap"] in (h["rootHeap"], h["systemHeap"]) for a in allocations),
            "reboot profile allocation heap invalid")
    root_count = sum(a["heap"] == h["rootHeap"] for a in allocations)
    require(all(a["heap"] == h["rootHeap"] for a in allocations[:root_count]),
            "reboot profile heap traversal order invalid")
    for heap_name, prefix in (("rootHeap", "root"), ("systemHeap", "system")):
        group = [a for a in allocations if a["heap"] == h[heap_name]]
        require((group[0]["node"] if group else 0) == h[prefix + "UsedHead"] and
                (group[-1]["node"] if group else 0) == h[prefix + "UsedTail"],
                "reboot profile/header used-list anchors disagree")
        spans = []
        start, end = h[heap_name + "Start"], h[heap_name + "End"]
        for i, a in enumerate(group):
            node, size, tag = a["node"], a["bytes"], a["tag"]
            padding = (tag >> 8) & 0x7F
            require(not (node | size | padding) & 3 and inside(node, 16, start, end) and
                    tag >> 16 == 0x484D and padding <= node - start and size <= end - node - 16 and
                    a["previous"] == (group[i - 1]["node"] if i else 0) and
                    a["next"] == (group[i + 1]["node"] if i + 1 < len(group) else 0),
                    "reboot profile allocation record invalid")
            spans.append((node - padding, node + 16 + size))
        spans.sort()
        require(all(a[1] <= b[0] for a, b in zip(spans, spans[1:])),
                "reboot profile same-heap allocation overlap")
    profile["allocations"] = allocations
    return profile


def inspect_bytes(data: bytes, path="<memory>") -> Archive:
    require(64 <= len(data) <= 64 + PAYLOAD_MAX, "archive size outside bounded envelope")
    outer = dict(zip(ENVELOPE_FIELDS, words(data, 0, 16)))
    require((outer["magic"], outer["headerSize"], outer["gameId"]) ==
            (0x4C4D5341, 64, 0x474C4D4A) and outer["version"] in (1, 2), "unsupported archive envelope/game")
    version = outer["snapshotVersion"]
    require(version in SUPPORTED_FORMATS, "only snapshot formats 24 through 28 are supported")
    static_ranges, camera_offset, game_offset = schema(version)
    if outer["version"] == 2:
        require(version >= 25 and outer["session"] != 0 and outer["reserved0"] == 0x4C4D5031 and
                outer["reserved1"] != 0, "invalid v2 profile/key/config envelope")
    else:
        require(not outer["reserved0"] and not outer["reserved1"], "nonzero envelope reserved words")
    trailer_size = TRAILER_SIZE + (PROFILE_SIZE if version >= 25 else 0)
    require(outer["trailerSize"] == trailer_size and
            outer["rawSize"] >= game_offset + 64 and
            outer["rawSize"] + trailer_size == outer["payloadSize"] == len(data) - 64,
            "archive raw/trailer/file extents disagree")
    payload = memoryview(data)[64:]
    require(zlib.crc32(payload) == outer["payloadCrc"], "archive payload CRC mismatch")
    h = dict(zip(HEADER_FIELDS, words(payload, 0, 64)))
    require((h["magic"], h["version"], h["headerSize"], h["gameId"]) ==
            (0x4C4D5354, version, 0x100, 0x474C4D4A), "unsupported snapshot header")
    require((h["heapMetadataOffset"], h["heapMetadataSize"], h["stateStaticsOffset"],
             h["stateStaticsSize"], h["heapDataOffset"]) ==
            (0x100, 0x48, STATICS_OFFSET, camera_offset - STATICS_OFFSET, game_offset),
            "format-24/25/26/27/28 section offsets disagree")
    core = h["totalSize"]
    require(h["heapSize"] > 0 and core == game_offset + h["heapSize"] and
            h["heapDataSize"] == h["heapSize"] and not core & 31 and
            game_offset < core <= SHARED_LIMIT - 64 and core + 64 <= outer["rawSize"],
            "core extent invalid")
    require(h["generation"] == outer["generation"], "archive/core generation mismatch")
    require(crc_zeroed(payload[:core], ((0, 4), (20, 24))) == h["checksum"], "core CRC mismatch")
    for prefix in ("rootHeap", "systemHeap", "heap"):
        start, end, size = (h[prefix + suffix] for suffix in ("Start", "End", "Size"))
        require(not (start | end) & 15 and size == end - start and
                inside(start, size, MEM1_START, MEM1_END) and
                not h[prefix] & 3 and inside(h[prefix], 0x84, MEM1_START, MEM1_END),
                f"{prefix} extent invalid")
    require(len({h["rootHeap"], h["systemHeap"], h["heap"]}) == 3, "heap objects are not distinct")
    require(inside(h["heapStart"], h["heapSize"], h["rootHeapStart"], h["rootHeapEnd"]) and
            inside(h["systemHeapStart"], h["systemHeapSize"], h["rootHeapStart"], h["rootHeapEnd"]) and
            (h["systemHeapEnd"] <= h["heapStart"] or h["heapEnd"] <= h["systemHeapStart"]),
            "child heap nesting/overlap invalid")
    require(h["currentHeap"] in (h["heap"], h["systemHeap"], h["rootHeap"]) or
            (not h["currentHeap"] & 3 and inside(h["currentHeap"], 0x84, h["heapStart"], h["heapEnd"])),
            "current heap invalid")
    require(inside(h["missionMode"], 0x1C, h["heapStart"], h["heapEnd"]) and
            h["gameMode"] == h["missionMode"] and h["gameModeCount"] == 1 and
            h["mainLoopMode"] == 2 and h["mainLoopPendingScene"] == h["sceneValue"] and
            h["mainDrawState"] <= 7 and not h["currentScene"] & 3 and
            inside(h["currentScene"], 4, MEM1_START, MEM1_END) and h["audioBasic"] == 0x803E3CF8,
            "snapshot gameplay identity invalid")
    for field in ("heapMode", "heapGroup", "rootHeapMode", "rootHeapGroup", "systemHeapMode",
                  "systemHeapGroup", "currentHeapGroup"):
        require(h[field] <= 255, f"{field} invalid")
    descriptor = words(payload, core, 16)
    magic, kind, generation, core_crc, packed_size, checksum = descriptor[:6]
    shared = dict(zip(SHARED_FIELDS, descriptor[6:]))
    require((magic, kind, generation, core_crc) == (0x4C4D5343, 2 if version >= 28 else 1,
            h["generation"], h["checksum"]),
            "companion/core binding invalid")
    require(0 < packed_size <= SHARED_LIMIT - core - 64, "companion packed extent invalid")
    packed_end = core + 64 + packed_size
    stored = core + 64 + ((packed_size + 31) & ~31)
    require(stored == outer["rawSize"] and stored <= SHARED_LIMIT, "companion stored extent invalid")
    require(shared["size"] == SHARED_SIZE and shared["systemHeap"] == h["systemHeap"] and
            shared["systemStart"] == h["systemHeapStart"] and shared["systemEnd"] == h["systemHeapEnd"],
            "companion SYS descriptor binding invalid")
    require(inside(shared["base"], SHARED_SIZE, shared["systemStart"], shared["systemEnd"]) and
            inside(shared["owner"], 0x68, shared["systemStart"], shared["systemEnd"]) and
            not (shared["base"] | shared["owner"]) & 3, "companion resource/owner extent invalid")
    require(crc_zeroed(payload[core:packed_end], ((20, 24),)) == checksum, "companion CRC mismatch")
    require(not any(payload[packed_end:stored]), "nonzero companion alignment padding")
    packed_parent = bytes(payload[core + 64:packed_end])
    parent = decode_parent(packed_parent, version >= 28)
    facts = {f"header.{key}": value for key, value in h.items()
             if key not in ("magic", "checksum", "headerSize", "totalSize", "randomState")}
    facts.update({f"shared.{key}": value for key, value in shared.items()})
    facts["shared.payloadCrc"] = zlib.crc32(parent)
    facts["shared.codec"] = "LML4" if packed_parent[:4] == b"LML4" else "zlib"
    facts["archive.buildCrc"], facts["archive.envelopeVersion"] = outer["buildCrc"], outer["version"]
    facts["archive.keyId" if outer["version"] == 2 else "archive.session"] = outer["session"]
    if outer["version"] == 2:
        facts["archive.configId"] = outer["reserved1"]
    static_offsets = []
    offset = STATICS_OFFSET
    for label, address, size in static_ranges:
        static_offsets.append((address, size, offset))
        facts[f"static.{label}.crc"] = zlib.crc32(payload[offset:offset + size])
        offset += size
    require(offset == camera_offset, "offline format manifest does not match camera offset")

    def read(address, size=4):
        if inside(address, size, h["heapStart"], h["heapEnd"]):
            pos = game_offset + address - h["heapStart"]
            return payload[pos:pos + size]
        for start, length, position in static_offsets:
            if inside(address, size, start, start + length):
                pos = position + address - start
                return payload[pos:pos + size]
        if inside(address, size, shared["base"], shared["base"] + SHARED_SIZE):
            pos = address - shared["base"]
            return memoryview(parent)[pos:pos + size]
        raise ArchiveError(f"address {address:08X} is not captured")

    # Same bounded HM-list invariants as lm_exp_heap.h, applied to captured GAME.
    require(words(payload, 0x138, 4) == tuple(h[key] for key in
            ("freeHead", "freeTail", "usedHead", "usedTail")), "GAME header/captured list anchors disagree")
    allocations, all_spans, total = [], [], 0
    for used in (True, False):
        node = h["usedHead" if used else "freeHead"]
        tail = h["usedTail" if used else "freeTail"]
        previous, previous_end, count = 0, h["heapStart"], 0
        while node:
            count += 1
            require(count <= 8192 and not node & 3 and inside(node, 16, h["heapStart"], h["heapEnd"]),
                    "GAME allocation list has invalid/cyclic endpoint")
            tag, size, back, following = words(read(node, 16), 0, 4)
            pad = (tag >> 8) & 0x7F if used else 0
            require(tag >> 16 == (0x484D if used else 0) and not (size | pad) & 3 and
                    pad <= node - h["heapStart"] and size <= h["heapEnd"] - node - 16 and
                    back == previous and (used or node >= previous_end), "GAME allocation metadata invalid")
            span = (node - pad, node + 16 + size)
            total += span[1] - span[0]
            require(total <= h["heapSize"], "GAME allocation accounting overflow")
            all_spans.append(span)
            if used:
                allocations.append((node + 16, size, tag))
            previous, previous_end, node = node, span[1], following
        require(previous == tail, "GAME allocation tail mismatch")
        facts[f"game.{ 'used' if used else 'free' }Blocks"] = count
    require(total == h["heapSize"], "GAME allocation accounting mismatch")
    ordered = sorted(all_spans)
    require(all(left[1] <= right[0] for left, right in zip(ordered, ordered[1:])), "GAME allocation spans overlap")
    warnings = ["CRC integrity verified; SipHash authentication NOT verified (keys intentionally not read).",
                "Saved facts only: matching values do not prove cross-boot portability or live compatibility.",
                "Audio bootstrap handle and general SYS/ROOT allocation headers are not captured."]
    camera_targets = set()
    for index in range(3):
        position = camera_offset + index * 0x100
        target, captured = words(payload, position, 2)
        require(inside(target, 0xEC, h["rootHeapStart"], h["rootHeapEnd"]) and target not in camera_targets,
                "camera target invalid or duplicated")
        camera_targets.add(target)
        in_game = inside(target, 0xEC, h["heapStart"], h["heapEnd"])
        require(captured == (0 if in_game else 0xEC), "camera capture mode disagrees with owning range")
        require(words(read(0x80399BE0 + index * 4), 0, 1)[0] == target, "camera record/static owner disagreement")
        image = read(target, 0xEC) if in_game else payload[position + 8:position + 8 + 0xEC]
        label = f"camera.{index}"
        facts.update({label + ".target": target, label + ".capturedBytes": captured,
                      label + ".objectCrc": zlib.crc32(image), label + ".firstWord": words(image, 0, 1)[0]})
        owner = next((block for block in allocations if inside(target, 0xEC, block[0], block[0] + block[1])), None)
        if in_game:
            require(owner is not None, "GAME camera has no containing captured used allocation")
            facts.update({label + ".allocationBase": owner[0], label + ".allocationSize": owner[1],
                          label + ".allocationTag": owner[2], label + ".allocationOffset": target - owner[0]})
        else:
            facts[label + ".allocationEvidence"] = "unavailable: allocator header/list not captured"
    for name, address in (("matrixArray", 0x804A17B8), ("booleanArray", 0x804A17BC),
                          ("sharedArchive", 0x804A12B0), ("depthBuffer", 0x803C4C14),
                          ("roomPropPicture0", 0x803C1C60), ("dialoguePicture0", 0x803C4140)):
        facts[f"owner.{name}"] = words(read(address), 0, 1)[0]
    volume = words(payload, stored, 10)
    resource = words(payload, stored + 0x1828, 49)
    model = words(payload, stored + 0x18EC, 5)
    require(volume[0] == resource[0] == model[0] == h["generation"] and volume[3] <= 64 and
            resource[3] <= 7 and resource[4] <= 24, "census generation/count bounds invalid")
    for label, values, names in (("volume", volume, "generation valid fault count head tail currentVolume currentDirId signature stableFrames"),
                                ("resource", resource[:11], "generation valid fault slotCount wantedCount recordBase bulkBase slotSize mapHash markMask backingBadMask"),
                                ("model", model, "generation valid fault signature registrySignature")):
        facts.update({f"census.{label}.{name}": value for name, value in zip(names.split(), values)})
    facts["census.resource.activeIds"] = list(resource[11:18])
    facts["census.resource.wantedIds"] = list(resource[25:25 + resource[4]])
    names = []
    for index in range(volume[3]):
        entry = payload[stored + 40 + index * 96:stored + 40 + (index + 1) * 96]
        raw_name = bytes(entry[76:92]).split(b"\0", 1)[0]
        names.append(raw_name.decode("ascii", "replace"))
    facts["census.volume.names"] = names
    profile = decode_profile(payload[stored + TRAILER_SIZE:], outer, h) if version >= 25 else None
    facts["profile.present"] = int(profile is not None)
    if profile:
        warnings[2] = "Retained allocation metadata is recorded; live SYS contents, hardware and audio readiness are NOT verified."
        for name in PROFILE_FIELDS:
            facts[f"profile.{name}"] = profile[name]
        for name, value in profile["anchors"].items():
            facts[f"profile.anchor.{name}"] = value
        for prefix, heap in (("root", h["rootHeap"]), ("system", h["systemHeap"])):
            facts[f"profile.{prefix}Allocations"] = sum(a["heap"] == heap for a in profile["allocations"])
        for i, record in enumerate(profile["allocations"]):
            for name, value in record.items():
                facts[f"profile.allocation.{i}.{name}"] = value
    return Archive(path, hashlib.sha256(data).hexdigest(), outer, h, shared, facts, warnings, profile)


def inspect_archive(path: Path):
    # A bounded read also handles a file growing after stat without a huge allocation.
    with path.open("rb") as source:
        data = source.read(PAYLOAD_MAX + 65)
    return inspect_bytes(data, str(path))


def compare(left: Archive, right: Archive):
    same_build = left.envelope["buildCrc"] == right.envelope["buildCrc"]
    return {"controlledBuildComparison": same_build,
            "warning": ("Same build only; this is NOT a portability proof." if same_build else
                        "DIFFERENT BUILDS: uncontrolled comparison; do not attribute differences to reboot alone."),
            "differences": [{"field": key, "left": left.facts.get(key), "right": right.facts.get(key)}
                            for key in sorted(left.facts.keys() | right.facts.keys())
                            if left.facts.get(key) != right.facts.get(key)]}


def display(value):
    return f"0x{value:08X}" if isinstance(value, int) else json.dumps(value, ensure_ascii=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archives", type=Path, nargs="+", help="One or two format-24/25 .lms files")
    parser.add_argument("--json", action="store_true", help="Machine-readable saved facts")
    parser.add_argument("--limit", type=int, default=40, help="Maximum printed differences (1..200)")
    args = parser.parse_args(argv)
    if len(args.archives) not in (1, 2) or not 1 <= args.limit <= 200:
        parser.error("provide one or two archives and a limit from 1 to 200")
    try:
        archives = [inspect_archive(path) for path in args.archives]
    except (ArchiveError, OSError) as error:
        print(f"INVALID: {error}", file=sys.stderr)
        return 2
    comparison = compare(*archives) if len(archives) == 2 else None
    if args.json:
        print(json.dumps({"archives": [a.__dict__ for a in archives], "comparison": comparison}, indent=2))
    else:
        for archive in archives:
            identity = "SD key ID" if archive.envelope["version"] == 2 else "session"
            print(f"{archive.path}: format {archive.header['version']}, envelope v{archive.envelope['version']}, "
                  f"build {archive.envelope['buildCrc']:08X}, {identity} {archive.envelope['session']:08X}, "
                  f"generation {archive.header['generation']}")
            print("  Envelope/core/companion CRCs, bounded decode, census and saved GAME lists: checked.")
        for warning in archives[0].warnings:
            print(warning)
        if comparison:
            print(comparison["warning"])
            differences = comparison["differences"]
            for row in differences[:args.limit]:
                print(f"  {row['field']}: {display(row['left'])} -> {display(row['right'])}")
            print(f"{len(differences)} differing fields; {max(0, len(differences) - args.limit)} not printed.")
        else:
            for key, value in archives[0].facts.items():
                if key.startswith(("shared.", "camera.", "owner.", "profile.")):
                    print(f"  {key}: {display(value)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
