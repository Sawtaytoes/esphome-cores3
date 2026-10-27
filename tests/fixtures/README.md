# Synthetic decoder fixtures

Generated with Pillow from a 320x240 RGB image containing equal red, green, blue,
and yellow vertical bars, saved as JPEG quality75. The progressive fixture uses
progressive=True; wrong-size is resized to640x480. Truncated retains the first
half plus EOI. Invalid-entropy retains the SOS header then32 FF00 pairs and EOI.
No live camera image or household data is included.
