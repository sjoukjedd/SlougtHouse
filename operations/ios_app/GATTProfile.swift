import CoreBluetooth

enum VUAMSUUID {
    static let service = CBUUID(string: "A5D5B001-5A5A-4B4B-8888-1A2B3C4D5E6F")
    static let aBlock  = CBUUID(string: "A5D5B002-5A5A-4B4B-8888-1A2B3C4D5E6F") // ECG
    static let iBlock  = CBUUID(string: "A5D5B003-5A5A-4B4B-8888-1A2B3C4D5E6F") // ICG
    static let mBlock  = CBUUID(string: "A5D5B004-5A5A-4B4B-8888-1A2B3C4D5E6F") // IMU
    static let pBlock  = CBUUID(string: "A5D5B005-5A5A-4B4B-8888-1A2B3C4D5E6F") // PPG
    static let sBlock  = CBUUID(string: "A5D5B006-5A5A-4B4B-8888-1A2B3C4D5E6F") // SCL
    static let tBlock  = CBUUID(string: "A5D5B007-5A5A-4B4B-8888-1A2B3C4D5E6F") // Temp
    static let status  = CBUUID(string: "A5D5B009-5A5A-4B4B-8888-1A2B3C4D5E6F")
    static let control = CBUUID(string: "A5D5B00A-5A5A-4B4B-8888-1A2B3C4D5E6F")

    static let allBlockUUIDs: [CBUUID] = [aBlock, iBlock, mBlock, pBlock, sBlock, tBlock]
    static let allUUIDs: [CBUUID] = allBlockUUIDs + [status, control]
}
