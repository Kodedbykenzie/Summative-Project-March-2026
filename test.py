"""Exercise every vibration API: normal data, empty list, invalid types."""

import vibration

samples = [1.0, 2.0, 3.0, 4.0]

print("=== Normal sample ===")
print("peak_to_peak:", vibration.peak_to_peak(samples))
print("rms:", vibration.rms(samples))
print("std_dev:", vibration.std_dev(samples))
print("above_threshold(2.5):", vibration.above_threshold(samples, 2.5))
print("summary:", vibration.summary(samples))

print("\n=== Empty list ===")
print("summary([]):", vibration.summary([]))
print("peak_to_peak([]):", vibration.peak_to_peak([]))
print("rms([]):", vibration.rms([]))

print("\n=== Invalid input (expect TypeError) ===")
try:
    vibration.peak_to_peak([1, 2.0])
except TypeError as e:
    print("Caught:", e)

try:
    vibration.rms("not a list")
except TypeError as e:
    print("Caught:", e)
