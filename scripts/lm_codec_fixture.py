"""Read private LM companion fixtures in memory; never export captured bytes/keys."""
from dataclasses import dataclass
import hashlib
from pathlib import Path

try:
    from scripts import compare_lm_archives as archives
except ModuleNotFoundError:
    import compare_lm_archives as archives


@dataclass(frozen=True)
class CompanionFixture:
    parent: bytes
    core: bytes
    original_packed: bytes
    archive_sha256: str
    format: int
    payload_limit: int
    trailer_size: int
    descriptor: bytes
    trailer: bytes

    @property
    def identity(self):
        return {"archive_sha256": self.archive_sha256,
                "parent_sha256": hashlib.sha256(self.parent).hexdigest(),
                "format": self.format, "parent_bytes": len(self.parent),
                "raw_core_bytes": len(self.core),
                "original_packed_bytes": len(self.original_packed),
                "trailer_bytes": self.trailer_size,
                "companion_capacity_bytes": archives.SHARED_LIMIT - len(self.core) - 64}


def load_companion(path: Path) -> CompanionFixture:
    # Parsing checks CRCs, exact extents, saved census and heap lists, not SipHash.
    with path.open("rb") as stream:
        data = stream.read(archives.PAYLOAD_MAX + 65)
    parsed = archives.inspect_bytes(data)
    if parsed.header["version"] != 27:
        raise archives.ArchiveError("benchmark requires a current format-27 companion")
    core_size = parsed.header["totalSize"]
    packed_size = archives.words(data, 64 + core_size + 16, 1)[0]
    start = 64 + core_size + 64
    packed = data[start:start + packed_size]
    parent = archives.decode_parent(packed)
    return CompanionFixture(parent, data[64:64 + core_size], packed,
                            parsed.sha256, 27, archives.SHARED_LIMIT,
                            parsed.envelope["trailerSize"],
                            data[64 + core_size:64 + core_size + 64],
                            data[64 + parsed.envelope["rawSize"]:])
