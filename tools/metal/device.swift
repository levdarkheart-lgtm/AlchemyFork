import Foundation
import Metal

// Read-only GPU inventory. Do not assume a feature exists because of the SDK version.
guard let device = MTLCreateSystemDefaultDevice() else {
    fputs("No Metal device available\n", stderr)
    exit(2)
}
var info: [String: Any] = [
    "name": device.name,
    "registryID": String(device.registryID),
    "recommendedMaxWorkingSetSize": device.recommendedMaxWorkingSetSize,
    "isLowPower": device.isLowPower,
    "isRemovable": device.isRemovable,
    "hasUnifiedMemory": device.hasUnifiedMemory
]
if #available(macOS 11.0, *) {
    info["supportsApple7"] = device.supportsFamily(.apple7)
}
if #available(macOS 12.0, *) {
    info["supportsApple8"] = device.supportsFamily(.apple8)
}
if #available(macOS 14.0, *) {
    info["supportsApple9"] = device.supportsFamily(.apple9)
}
let data = try JSONSerialization.data(withJSONObject: info, options: [.prettyPrinted, .sortedKeys])
print(String(data: data, encoding: .utf8)!)
