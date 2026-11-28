# Datasheet check — pre-order hardware review

Date: 2026-11-28. Each answer below comes from a datasheet or product page downloaded in this session, except where it says otherwise. LCSC datasheet links are the `datasheet.lcsc.com` PDFs linked from each LCSC product page.

---

## 1. MPM3610 (MPS, LCSC C5183856): EN, AAM, VCC, capacitors

**Answer:** No. The 1700 netlist ties U1-17 (EN) and U1-16 (IN) to V_12, and the datasheet does not allow EN to be driven directly from 12 V. EN has an internal 6.5 V Zener clamp. Wired straight to a source, that source must be ≤ 6 V. From VIN you need a pull-up of at least 55 kΩ at 12 V (all MPS reference circuits use 100 kΩ from VIN to EN). Leaving AAM (U1-18) and VCC (U1-2) unconnected is allowed: VCC has an integrated output capacitor and needs nothing, and floating AAM gives forced CCM. Capacitors: 10 µF input and 22 µF output (ceramic) in every reference circuit.

**Figures (quoted):**
- Enable Control: "EN is clamped internally using a 6.5V series-Zener-diode." "Connecting EN to a voltage source directly without a pull-up resistor requires limiting the amplitude of the voltage source to ≤6V to prevent damage to the Zener diode." "Connecting the EN input through a pull-up resistor to the voltage on the VIN pin limits the EN input current to less than 100µA. For example, with 12V connected to Vin, RPULLUP ≥ (12V – 6.5V) ÷ 100µA = 55kΩ."
- Abs max: "All Other Pins … -0.3V to 6V (2)", where note 2 says "For additional details on EN pin's ABS MAX rating, please refer to the 'Enable Control' section".
- "An internal 1MΩ resistor from EN to GND allows EN to be floated to shut down the chip." EN rising threshold 1.15/1.4/1.65 V.
- Pin 2 VCC: "Internal 4.9V LDO Output. The module integrates a LDO output capacitor, so there is no need to add an external capacitor."
- Pin 18 AAM: "Float AAM or drive AAM high (>2.5V) to force the MPM3610 to operate in continuous conduction mode (CCM). If AAM mode is required under light load, connect a resistor to ground to program AAM voltage in the range of 0V to 1V." The reference circuits use R4 = 53.6 kΩ (5 V out) or 39 kΩ (3.3 V out).
- Pins 10, 15, 19, 20 NC: "DO NOT CONNECT. NC must be left floating." Pin 11 BST needs no external connection. The board leaves 10, 11 and 15 N/C, which matches.
- Input capacitor: "For most applications, use a 10µF capacitor" (X5R/X7R ceramic, placed close to IN).
- Output: "An output capacitor is needed." Figures 11 to 17 (12 V in): C1 = 10 µF, C2 = 22 µF, R3 = 100k from VIN to EN.
- VIN operating range 4.5 V to 21 V (abs max 28 V).

**Source:** MPM3610 datasheet Rev. 1.01 (1/7/2015), https://datasheet.lcsc.com/datasheet/pdf/8eab35c1977d771c0d9696297c1afcd7.pdf (linked from https://jlcpcb.com/partdetail/C5183856. The LCSC product page for C5183856 returned "Page Not Found", and monolithicpower.com served only a JavaScript challenge page).

**Confidence:** Read from datasheet.

---

## 2. CH334F (WCH, LCSC C5187527): crystal, floating pins, upstream VBUS

**Answer:**
- **Crystal:** The load capacitors are built in (about 16 pF on XI and XO), and WCH recommends adding none. The board's crystal (X1/X2 = YXC X322512MSB4SI, C9002) is listed on LCSC at 20 pF load capacitance. The datasheet only says "no external caps", so there is a small mismatch between 20 pF and about 16 pF internal. That is normally fine for USB tolerance, but it is not stated as checked.
- **Floating pins:** RESET#/CDP, PSELF and OVCUR# all have internal pull-ups and may float. WCH recommends leaving RESET# fully floating when unused. PSELF floating (high) selects self-powered. SDA has an internal pull-up current of about 250 µA, so no external pull-up is needed. The LED pins may be left unconnected: the datasheet supports "no LED" applications.
- **Upstream VBUS:** The CH334F has no VBUS-sense pin, so the hub does not need upstream VBUS to run. It is powered from V5 (5 V or 3.3 V). The datasheet's self-power section only warns against back-feeding the external supply into the host's VBUS. Leaving the upstream VBUS unconnected avoids that problem.

**Figures (quoted, translated from Chinese):**
- Features: "提供晶体振荡器，内置电容" ("crystal oscillator provided, capacitors built in"), 12 MHz.
- §6.1, below the reference schematic: "XI 和 XO 引脚已内置约 16pF 振荡电容，建议晶体 X1 不加外部振荡电容" ("XI and XO already have about 16 pF oscillator capacitors built in. Do not add external capacitors to crystal X1.").
- §6.2: crystal-less mode is selected only by floating XO and tying XI to GND, and is not enabled by default on some packages.
- RESET#/CDP (CH334F pin 16): "外部复位输入，内置上拉电阻，低电平有效 … 不复位时建议完全悬空" ("external reset input, internal pull-up, active low … leave fully floating if not used"). §3.2.2: "已内置约 25KΩ上拉电阻" ("about 25 kΩ pull-up built in"). The chip also has its own power-on reset (Trpor about 5 to 14 ms). Driving it high at power-up enables CDP mode.
- PSELF (pin 18): "配置供电模式，内置上拉电阻：默认高电平为自供电，低电平设置为总线供电" ("power-mode select, internal pull-up; default high = self-powered, low = bus-powered").
- OVCUR# (pin 1): "HUB 芯片的 OVCUR#引脚提供内置的弱上拉电流，所以可省掉 R11～R14" ("OVCUR# has a built-in weak pull-up current, so R11 to R14 may be omitted"). It is a 5 V-tolerant input.
- SDA: "SDA 引脚已内置约 250uA 上拉电流以支持开漏双向数据通讯，无需外部上拉电阻" ("SDA has about 250 µA of built-in pull-up current … no external pull-up resistor needed").
- §3.3.3: "对于 CH334U 和 CH334F，有独立的 PSELF 引脚 … 引脚 LED1 和 LED2 无需复用于配置" ("on CH334U and CH334F, PSELF is a separate pin … LED1 and LED2 are not needed for configuration").
- Power pins: V5 needs "外接 1uF 或更大电容" (1 µF or more); VDD33 needs "0.1uF+10uF 退耦电容，或 1uF".
- Upstream DPU has a built-in 1.5 kΩ pull-up, and the downstream pull-downs are built in.
- CH334F pin map (4F column): XO 3, XI 4, DMU 14, DPU 15, RESET# 16, PSELF 18, V5 19, VDD33 20, OVCUR# 1, LED3/SCL 13, LED4/SDA 21, PWREN# 24. The 1700 netlist matches this: X1 on pins 3/4, +5 V on 19, 3.3V_USB on 20.

**Source:** CH334/CH335 datasheet V2.91, https://datasheet.lcsc.com/datasheet/pdf/77721fceaa29165ba79c58173b46c67f.pdf (from https://www.lcsc.com/product-detail/C5187527.html). Crystal load capacitance from https://www.lcsc.com/product-detail/C9002.html.

**Confidence:** Read from datasheet. The VBUS answer is inferred from the pin list (no VBUS pin) and the §3.6 self-power text. The datasheet has no explicit sentence saying "VBUS not required".

---

## 3. KEC 2N3904S-RTK/PS (LCSC C18536): SOT-23 pinout

**Answer:** Yes. This KEC part is 1 = Emitter, 2 = Base, 3 = Collector, which is the board symbol's order. KC-01-1872 Q1 to Q5 have pin 1 on GND, as expected for the emitter. Note that this differs from the more common SOT-23 BJT order (1 B, 2 E, 3 C), so do not substitute another maker's 2N3904 SOT-23 without checking.

**Figures:** Package drawing legend: "1. EMITTER 2. BASE 3. COLLECTOR", SOT-23, marking "ZC". VCEO 40 V, IC 200 mA, PC 350 mW.

**Source:** KEC 2N3904S technical data Rev. 3 (2003-02-25), https://datasheet.lcsc.com/datasheet/pdf/72a53966107c48ec8ea76d7e2accf63a.pdf (from https://www.lcsc.com/product-detail/C18536.html, page title "2N3904S-RTK/PS | … | KEC").

**Confidence:** Read from datasheet.

---

## 4. PB26-13M-B-WT-RGB-N / PB26-13M-E-RGB (Honyone): LED polarity, resistors, Vf/If, pinout

**Answer:** Could not read. No source for these parts was reachable:
- The AliExpress item pages (2251832813241589, 3256808407046596) are blocked by this session's network policy (proxy 403).
- The JLCPCB parts search returned no results through a headless browser. LCSC search pages return "Access Denied".
- The only PB26 listing found on LCSC, C2981366 (D-SWITCH / Dasung "PB26-M13-ZT-N-NN"), is a non-illuminated version. Its drawing shows only the switch contacts (SPDT, COM/NO/NC, 1 A 125 VAC / 0.5 A 250 VAC). It gives nothing on the RGB LED, so it does not answer this question.

Common-anode vs. common-cathode, built-in resistors, Vf/If and LED pin numbering are therefore **unverified**. Board wiring from the KC-01-1802 netlist, for whoever checks a sample: SW1 pin 1 = +5V, pin 2 = BUTTON00, pin 3 = N/C, pin 4 = NNAME39, pin 5 = +5V, pin 6 = NNAME41, pin 7 = NNAME40. The design needs a **common-anode** LED on pin 1/5 with cathodes on 4/7/6 (R/G/B). It also assumes no built-in resistors are needed for the about 18.5 mA WS2811 constant-current sinks; built-in resistors would only reduce the WS2811 headroom.

Recommendation: before ordering, get the Honyone drawing from the seller, or measure one sample in diode mode from pin 1 to pins 4, 6 and 7.

**Source:** Attempted: https://www.aliexpress.com/item/2251832813241589.html and https://www.aliexpress.com/item/3256808407046596.html (blocked). Read, but not applicable: https://www.lcsc.com/product-detail/C2981366.html with datasheet https://datasheet.lcsc.com/datasheet/pdf/392f91a8a88118718ac008c7fd589a59.pdf.

**Confidence:** Could not read.

---

## 5. Panel-mount 5.5 × 2.1 mm DC jack for 6 mm acrylic

**Answer:** None of the Same Sky panel jacks has a long enough thread for 6 mm acrylic. They have a 5 A rating, 24 Vdc, and a 2.0 mm pin (the "A" versions, sold as the 2.1 mm-plug fit) or a 2.5 mm pin ("B"). Their datasheets give no maximum panel thickness or cut-out, so the figures below are worked out from the drawings. PJ-005A (solder lugs) has a 5.7 mm thread. PJ-065A, the long-barrel version, has an 8.0 mm thread minus a 2.0 mm nut and 0.5 mm washer, so about 5.5 mm. A 6 mm panel needs a counterbore on the inside face, or a jack with a longer bushing. Adafruit 610 states panel thickness up to 8 mm, but gives no current rating.

| Candidate | Centre pin | Rated current | Thread / panel thickness | Cut-out | Terminals |
|---|---|---|---|---|---|
| Same Sky PJ-005A | Ø2.0 mm | 5 A, 24 Vdc | 5/16"-32 thread, 5.7 mm long; nut 2 mm + washer 0.5 mm + bakelite washer, so about 3 mm panel max (derived) | Ø about 8 mm round for 5/16" thread (derived; flange Ø10.8) | 3 solder lugs |
| Same Sky PJ-065A (long barrel) | Ø2.0 mm | 5 A, 24 Vdc | 5/16"-32W thread, 8.00 mm long; nut 2.0 mm + washer 0.5 mm, so about 5.5 mm panel max (derived); 6 mm does not fit without a counterbore | Ø about 8 mm (derived; flange Ø10.8) | 3 lugs (1.2 × 2.5 mm slots) |
| Adafruit 610 | 2.1 mm plug fit | not stated | "panel up to 8mm thick" (stated) | not stated in the text read | 3 solder eyes |
| Adafruit 5607 (cable, see Q6) | 5.5/2.1 | not stated | "panel up to 16mm thick" (stated); M22 thread, 14 mm long (drawing) | "drill a 1" hole" (stated); drawing shows thread Ø20/22 | 20 AWG pigtail |

Also rated 5 A on Same Sky's site: PJ-066A and PJ-067A (horizontal panel mount, PCB pins) and PJ-064A (vertical, PC pins). These are not solder-lug wire jacks.

**Source:** https://www.sameskydevices.com/panel-mount-dc-power-jacks; https://www.sameskydevices.com/product/resource/pj-005a.pdf; https://www.sameskydevices.com/product/resource/pj-065x.pdf; https://www.sameskydevices.com/product/resource/pj-066x.pdf; https://www.sameskydevices.com/product/resource/pj-067a.pdf; https://www.sameskydevices.com/product/resource/pj-064x.pdf; https://www.adafruit.com/product/610.

**Confidence:** Read from datasheet for current, voltage and thread dimensions. The panel-thickness and cut-out figures for Same Sky are derived from the drawings, not stated. Not checked here: whether 2.0 mm-pin jacks mate well with 2.1 mm plugs (Same Sky only says "2.1 or 2.5 mm" on its catalog page).

---

## 6. Adafruit 5607 (round panel-mount 5.5/2.1 barrel extension, 30 cm)

**Answer:** No current rating anywhere on the product page or its two downloads. The cable drawing gives the wire as UL2464 20 AWG, 2 conductors.

**Figures:** Cable drawing PCM-0384, item 1: "UL2464 20AWG*2C+AE OD:4.6MM BLACK PVC JACKE." Items 2 and 3: "DC5521 PLUG/JACK WITH NICKLE PLATED BRASS". Test notes: "CONTACT RESISTANCE: 3ohm MAX", insulation resistance 10 MΩ min at DC 300 V. Pin assignment: centre to centre (red), shell to shell (black). Socket drawing: M22 thread, 14.00 mm thread length, flange Ø28, nut Ø30 × 8 mm. Product page: "threaded part is 23mm diameter … drill a 1" hole", "panel up to 16mm thick (or 0.625")". Cable 30 cm.

**Source:** https://www.adafruit.com/product/5607; https://cdn-shop.adafruit.com/product-files/5607/C17817_PCM-0384.pdf; https://cdn-shop.adafruit.com/product-files/5607/C17817-DC_Socket.pdf.

**Confidence:** Read from the product page and drawings. "No rating" means none is stated.

---

## 7. Adafruit 4218 (USB-C round panel-mount extension)

**Answer:** It fits panels from a few mm up to 16 mm thick, in a hole of 21.5 to about 27 mm. It carries CC1 (A5), B5, USB 2 D+/D− (A6/A7), all SuperSpeed pairs, SBU, VBUS and GND. **B6/B7 (the receptacle's second D+/D− pair) are not in the wire list.** A USB-C receptacle normally bridges A6/B6 and A7/B7 so that USB 2 works in both plug orientations. Without that, USB 2 data may only work with the plug one way round. Check one sample in both orientations. All conductors are 32 AWG, VBUS included. The drawing puts VBUS and GND contacts on one line ("A1+B1+A4+B4+A9+B9+A12+B12"), which is presumably a drafting shorthand, but the drawing does not show them as separate.

**Figures:** Product page: "The adapter can fit holes 21.5mm to ~27mm", "attach to a panel up to 16mm thick (or 0.625"), or as thin as a few mm." Drawing PCM-0165 pin assignment, P1 (C male) to P2 (C female): A2, A3, A5, A6, A7, A8, B8, A10, A11, B2, B3, B5, B11, B10, "A1+B1+A4+B4+A9+B9+A12+B12", SHELL. Cable: "(32AWG*1C+EAM)*4+32AWG*1P+32AWG*4C+2EAB(85%) OD:4.8MM L:300mm … UL2725". Item 3: "USB3.1 C TYPE FEMALE … SOLDER TYPE". Contact resistance 3 Ω max (test limit). The thread length is not dimensioned on the drawing.

**Source:** https://www.adafruit.com/product/4218; https://cdn-shop.adafruit.com/product-files/4218/P4218_C12430-001_PCM-0165_TYPE_C_F-_MTYPE_C_M__30CM.pdf.

**Confidence:** Read from the product page and drawing. The B6/B7 concern is my reading of the wire list, not a stated limitation.

---

## 8. DORABO DB127S-5.08 (C2997523), DB125-3.81-10P (C5137189), KEFA KF127S-3.81-4P (C2974912)

**Answer:** All three are fixed, one-piece PCB screw terminal blocks with the screw clamp and PCB pins in one housing. They are not pluggable headers and need no mating plug. LCSC categorises all three as "Screw Terminal Blocks", and each drawing shows one moulded part with solder pins and screw clamps.

| Part | Pitch | Rated current (UL / IEC) | Wire range | Screw / torque | Strip | Hole |
|---|---|---|---|---|---|---|
| DB127S-5.08-6P-GN-S | 5.08 mm | UL 12 A (B), 10 A (D); IEC 15 A; LCSC "12A" | 26 to 14 AWG; 1.5 mm² | M3, 0.5 N·m | 5 to 6 mm | Ø1.40 |
| DB125-3.81-10P-GN-S | 3.81 mm | UL 10 A; IEC 13.5 A; LCSC "10A" | 28 to 16 AWG; 1.0 mm² | M2, 0.2 N·m | 4 to 5 mm | Ø1.20 |
| KF127S-3.81-4P | 3.81 mm | UL 10 A; IEC 10 A; LCSC "10A" | 26 to 16 AWG; 1.0 mm² | M2 | 6 mm | Ø1.30 |

Rated voltage: UL 300 V for all three. IEC: DB127S 250/400/630 V, DB125 and KF127S 160/200/400 V.

**Source:** https://www.lcsc.com/product-detail/C2997523.html with https://datasheet.lcsc.com/datasheet/pdf/a975929a72505a5e9c82962a0037fa45.pdf; https://www.lcsc.com/product-detail/C5137189.html with https://datasheet.lcsc.com/datasheet/pdf/a389576cfd8bc49edc37f14f6bc66685.pdf; https://www.lcsc.com/product-detail/C2974912.html with https://datasheet.lcsc.com/datasheet/pdf/f7cf02806b77b672bff127e593c10d45.pdf.

**Confidence:** Read from the datasheet drawings (image PDFs, read visually at low resolution) and the LCSC parameter tables, which agree.

---

## 9. EMC2101 (Microchip, EMC2101-R-ACZL-TR, LCSC C626968): FAN pin

**Answer:** Yes, the FAN pin (pin 4) is open-drain and 5 V-tolerant in PWM mode. It sinks 8 mA at VOL ≤ 0.3 V. There are two conditions on using 5 V pull-ups. First, the pin-to-VDD difference must stay ≤ 3.6 V, which 5 V against 3.3 V VDD satisfies. Second, the datasheet says pull-ups on 5 V-tolerant pins must not exceed 3.6 V while the EMC2101 is unpowered. So the fans' internal 5 V pull-ups must not be live while the EMC2101's 3.3 V is off. The datasheet also says the PWM pull-up should go "to VDD". The 10 µA leakage spec is stated only for pull-up ≤ 3.6 V.

**Figures (quoted):**
- Pin table: "4 FAN OD (5V) PWM Output (default - software programmed) / AO DAC Output". "All pins labeled with (5V) are 5V tolerant. For the 5V tolerant pins that have a pull-up resistor, the voltage difference between VDD and the pull-up voltage must never exceed 3.6V."
- "OD: Open Drain Digital Output … It is open drain and requires a pull-up resistor. This pin is 5V tolerant."
- §4.6.2: "The PWM output is open drain and requires a pull-up resistor to VDD."
- Abs max: "Voltage on 5V tolerant pins … -0.3 to 5.5V"; "(|V5VT_pin − VDD|) … -0.3 to 3.6V"; "For the 5V tolerant pins that have a pull-up resistor, the pull-up voltage must not exceed 3.6V when EMC2101 is unpowered."
- Digital I/O (PWM, SMDATA, SMCLK, ALERT/TACH): "Output Low Voltage VOL — — 0.3 V 8 mA Current Sink"; "Output Leakage Current ILEAK 10 µA, device powered or unpowered, TA < +85°C, Pull-up voltage ≤ 3.6V".
- PWM frequency 22 Hz to 5 kHz at 64-step resolution; higher frequencies are possible with reduced resolution (Appendix B).
- POLARITY bit (reg 4Ah bit 4): on the EMC2101-R its default is set by the ALERT/TACH pull-up value (Table 4-1).

**Source:** EMC2101 datasheet (Microchip), https://datasheet.lcsc.com/datasheet/pdf/b7817b569455a76b903880391e53f892.pdf (from https://www.lcsc.com/product-detail/C626968.html).

**Confidence:** Read from datasheet.

---

## 10. 2N7002, IRF4905S, and the 33 kΩ 0402 resistor: pinouts, ratings, stock

**2N7002: JSCJ (Changjiang), LCSC C8545**
- Pinout confirmed: "1. GATE 2. SOURCE 3. DRAIN", SOT-23.
- VGS max ±20 V. VGS(th) 1.0 V min, 1.6 V typ, 2.5 V max (VDS = VGS, ID = 250 µA). VDS 60 V, ID 115 mA. RDS(on) 5 Ω max at VGS = 10 V / 500 mA, 7 Ω max at VGS = 5 V / 50 mA.
- Stock: LCSC page "4,988,650 in stock". JLCPCB: **Basic part** (`componentLibraryType: base`, "Basic" badge).
- Sources: https://www.lcsc.com/product-detail/C8545.html, https://datasheet.lcsc.com/datasheet/pdf/a141b8bd86b14475955ac8c4d3eea0a8.pdf, https://jlcpcb.com/partdetail/C8545.

**IRF4905S: Infineon IRF4905STRLPBF, LCSC C2620**
- D2PAK lead assignments (HEXFET): "1.- GATE 2, 4.- DRAIN 3.- SOURCE", where 4 is the tab. This confirms 1 G, 2/tab D, 3 S.
- RDS(on) 20 mΩ max at VGS = −10 V, ID = −42 A. VGS ±20 V. VGS(th) −2.0 to −4.0 V. V(BR)DSS −55 V. ID −70 A (silicon limited) / −42 A (package limited) at VGS 10 V.
- Stock: LCSC page "23,583 in stock". JLCPCB lists C2620 as an **Extended part** (`componentLibraryType: expand`). I could not reliably read the JLCPCB stock count from the page. A web-search snippet (not read directly) said 11,842 at JLCPCB, and that the second JLCPCB listing C386355 is "no longer manufactured", so use C2620.
- Sources: https://www.lcsc.com/product-detail/C2620.html, https://datasheet.lcsc.com/datasheet/pdf/9f9d3e577ee68bfc3709a91fcb1a82a9.pdf (IRF4905S/L datasheet, page 9 D2Pak outline), https://jlcpcb.com/partdetail/InfineonTechnologies-IRF4905STRLPBF/C2620.

**UNI-ROYAL 0402WGF3302TCE (33 kΩ 0402, 1%, 62.5 mW): LCSC C25779**
- LCSC page "1,227,500 in stock". JLCPCB: **Basic part** (`componentLibraryType: base`, "Basic" badge).
- Sources: https://www.lcsc.com/product-detail/C25779.html, https://jlcpcb.com/partdetail/C25779.

**Confidence:** Read from datasheets and product pages. Stock figures are from 2026-11-28 and will change.
