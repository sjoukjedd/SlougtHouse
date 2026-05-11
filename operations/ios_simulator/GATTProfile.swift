import CoreBluetooth

enum VUAMSUUID {
    static let service  = CBUUID(string: "A5D5B001-5A5A-4B4B-8888-1A2B3C4D5E6F")
    static let aBlock   = CBUUID(string: "A5D5B002-5A5A-4B4B-8888-1A2B3C4D5E6F")
    static let iBlock   = CBUUID(string: "A5D5B003-5A5A-4B4B-8888-1A2B3C4D5E6F")
    static let mBlock   = CBUUID(string: "A5D5B004-5A5A-4B4B-8888-1A2B3C4D5E6F")
    static let pBlock   = CBUUID(string: "A5D5B005-5A5A-4B4B-8888-1A2B3C4D5E6F")
    static let sBlock   = CBUUID(string: "A5D5B006-5A5A-4B4B-8888-1A2B3C4D5E6F")
    static let tBlock   = CBUUID(string: "A5D5B007-5A5A-4B4B-8888-1A2B3C4D5E6F")
    static let status   = CBUUID(string: "A5D5B009-5A5A-4B4B-8888-1A2B3C4D5E6F")
    static let control  = CBUUID(string: "A5D5B00A-5A5A-4B4B-8888-1A2B3C4D5E6F")
    static let xBlock   = CBUUID(string: "A5D5B00D-5A5A-4B4B-8888-1A2B3C4D5E6F")
    static let yBlock   = CBUUID(string: "A5D5B008-5A5A-4B4B-8888-1A2B3C4D5E6F")
    static let rBlock   = CBUUID(string: "A5D5B00E-5A5A-4B4B-8888-1A2B3C4D5E6F") // Respiratory rate
}
