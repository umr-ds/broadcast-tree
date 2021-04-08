-- Basic definition of the BTP. Taken from Sergio Dominguez' thesis.
btp_protocol = Proto("BTP",  "Broadcast Tree Protocol")

seq_num = ProtoField.uint32("btp.seq_num", "Seqence Number", base.DEC)
src_flg = ProtoField.uint16("btp.src_flg", "Source Flag", base.DEC)
ttl = ProtoField.uint16("btp.ttl", "Time-to-Live", base.DEC)
type = ProtoField.string("btp.type", "Type")
fin_flg = ProtoField.uint16("btp.fin", "Finished", base.DEC)
pwr_flg = ProtoField.uint8("btp.pwr_flg", "Power Flag", base.DEC)
route_len = ProtoField.uint8("btp.route_len", "Route Length", base.DEC)

used_tx_pwr = ProtoField.uint8("btp.used_tx_pwr", "Used TX Power", base.DEC)
max_tx_pwr = ProtoField.uint8("btp.max_tx_pwr", "Maximum TX Power", base.DEC)
snd_tx_pwr = ProtoField.uint8("btp.snd_tx_pwr", "2nd TX Power", base.DEC)
max_snr = ProtoField.int8("btp.max_snr", "Maximum SNR", base.DEC)

route_elem = ProtoField.ether("btp.route_address", "Address")

payload = ProtoField.bytes("btp.payload", "Payload")

btp_protocol.fields = {seq_num, src_flg, ttl, type, fin_flg, pwr_flg, route_len,
  used_tx_pwr, max_tx_pwr, snd_tx_pwr, max_snr, route_elem, payload
}

function btp_protocol.dissector(buffer, pinfo, tree)
  -- Ignore empty packets
  if buffer:len() == 0 then return end

  -- Set the protocol name and add a new subtree in wireshark.
  pinfo.cols.protocol = btp_protocol.name
  local subtree = tree:add(btp_protocol, buffer(), "Broadcast Tree Protocol")

  -- Start dessecting the packet.
  -- The first 4 bytes have to be skipped.
  subtree:add_le(seq_num, buffer(4,4))
  subtree:add_le(src_flg, buffer(8,2))
  subtree:add_le(ttl, buffer(10,2))

  -- We want to have some human readable string for the frame type
  -- So, extract it from the buffer, get the name using the function and the end
  -- and add it to the subtree using the following form:
  -- <HUMAN_READABLE_NAME> (type)
  local frame_type = buffer(12,2):le_uint()
  local type_name = get_frame_type_name(frame_type)
  subtree:add(type, type_name .. " (" .. frame_type .. ")")

  -- Continue dissection
  subtree:add_le(fin_flg, buffer(14,2))
  subtree:add_le(pwr_flg, buffer(16,2))
  subtree:add_le(route_len, buffer(18,2))

  -- We have two optional parts: the power data and route data
  -- We only have to dissect the power data, if it is available as
  -- indicated in the pwr_flg field.
  -- If it is available, dissect it. Skip otherwise.
  if buffer(16,2):le_uint() == 1 then
    local power_tree = subtree:add(btp_protocol, buffer(), "Power Data")
    power_tree:add_le(used_tx_pwr, buffer(20,1))
    power_tree:add_le(max_tx_pwr, buffer(21,1))
    power_tree:add_le(snd_tx_pwr, buffer(22,1))
    power_tree:add_le(max_snr, buffer(23,1))
  end

  -- The second optional part.
  -- If the route_len field is set, decode route_len times
  -- MAC addresses.
  local route_len_field = buffer(18,2):le_uint()
  if route_len_field > 0 then
    local route_tree = subtree:add(btp_protocol, buffer(), "Route Data")
    for i=1,route_len_field,1 do
      local address_offset = 24 + (i - 1) * 6
      route_tree:add(route_elem, buffer(address_offset,6))
    end
  end

  -- Finally, we have the payload.
  local payload_offset = 24 + (route_len_field) * 6
  subtree:add(payload, buffer(payload_offset))
end

-- Function for getting the human readable name for a frame type.
function get_frame_type_name(frame_type)
  local type_name = "Unknown"

      if frame_type == 0 then type_name = "Neighbour Discovery"
  elseif frame_type == 1 then type_name = "Child Request"
  elseif frame_type == 2 then type_name = "Parent Rejection"
  elseif frame_type == 3 then type_name = "Parent Revocation"
  elseif frame_type == 4 then type_name = "Parent Confirmation"
  elseif frame_type == 5 then type_name = "Local End"
  elseif frame_type == 6 then type_name = "Data" end

  return type_name
end

-- Add the dissector to the ethertype dissector.
-- Dissect BTP, if ethertype is 0x0101 (more or less randomly chosen).
local llc = DissectorTable.get("ethertype")
llc:add(0x0101, btp_protocol)
