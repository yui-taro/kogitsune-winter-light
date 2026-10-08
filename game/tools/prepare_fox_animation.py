"""Align the four generated poses in each fox animation strip.

Run with the bundled Python runtime (which includes Pillow):
    python3 tools/prepare_fox_animation.py
The unmodified generated sheets live in art-source/ for repeatable processing.
"""

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ANGLES = ("side", "front-right", "front", "back-right", "back")
MOTIONS = ("walk", "run", "crouch")
FRAMES = 4
PADDING = 8


def prepare(motion: str, angle: str) -> None:
    source = Image.open(ROOT / "art-source" / f"fox-{motion}-{angle}-source.png").convert("RGBA")
    if source.width % FRAMES:
        raise ValueError(f"{angle}: sheet width is not divisible by {FRAMES}")
    cell_width = source.width // FRAMES
    poses = []
    for index in range(FRAMES):
        frame = source.crop((index * cell_width, 0, (index + 1) * cell_width, source.height))
        # Ignore very faint stray pixels left by generation when finding the fox.
        alpha = frame.getchannel("A").point(lambda value: 255 if value > 128 else 0)
        box = alpha.getbbox()
        if box is None:
            raise ValueError(f"{angle}: empty frame {index}")
        left, top, right, bottom = box
        poses.append(frame.crop((max(0, left - 4), max(0, top - 4),
                                 min(cell_width, right + 4), min(source.height, bottom + 4))))

    width = max(pose.width for pose in poses) + PADDING * 2
    height = max(pose.height for pose in poses) + PADDING * 2
    atlas = Image.new("RGBA", (width * FRAMES, height), (0, 0, 0, 0))
    for index, pose in enumerate(poses):
        x = index * width + (width - pose.width) // 2
        y = height - PADDING - pose.height
        atlas.alpha_composite(pose, (x, y))
    output = ROOT / "assets" / f"fox-{motion}-{angle}-v1.png"
    atlas.save(output, optimize=True)
    print(f"{motion}/{angle}: {width}x{height} per frame -> {output}")


if __name__ == "__main__":
    for motion in MOTIONS:
        for angle in ANGLES:
            prepare(motion, angle)
