package statistics;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.HashMap;

public class Statistics
{
	enum FRAME_TYPE
	{
		CYCLE_CHECK,
		NEIGHBOR_DISCOVERY,
		CHILD_REQUEST,
		CHILD_CONFIRMATION,
		CHILD_REJECTION,
		PARENT_REVOCATION,
		END_OF_GAME,
		APPLICATION_DATA
	}
	
	
	public static void main(String[] args) throws Exception
	{
		try(BufferedReader br = new BufferedReader(
				new FileReader(args[0])))
		{
			boolean newRound = true, isNodeZero = false, hadSource = false;
			int unconnectedNodes = 0;
			double totalEnergy = 0, totalTx = 0, timeToFinish = 0;
			double[] energyPerFrame_rx = new double[8];
			double[] energyPerFrame_tx = new double[8];
			
			String line;
			String[] split;
			int lineCounter = 0;
			
			HashMap<String, ArrayList<String>> tree = new HashMap<>();
			
			Node currentNode = null;
			Section currentSection = null;
			ArrayList<Section> sections = new ArrayList<>();
			boolean energy = false, dataSent = false, dataRecv = false;
			
			while((line = br.readLine()) != null)
			{
				lineCounter++;
				
				if(line.equals("<========== END OF SIMULATION ==========>"))
				{
					currentSection = new Section();
					sections.add(currentSection);
					
					hadSource = false;
					
					Node n = new Node();
					n.setMacAddress("ff:ff:ff:ff:ff:ff");
					currentSection.addNode(n);
				}
				else if(line.startsWith("The parent of node"))
				{
					line = line.substring(19);
					split = line.split(" is ");
					
					Node node = new Node();
					node.setMacAddress(split[0].substring(1, split[0].length() - 1));
					currentSection.addNode(node);
					
					if(!hadSource)
					{
						currentSection.setSource(node);
						hadSource = true;
					}
				}
				else if(line.startsWith("<===================== Node"))
				{
					//currentNode = currentSection.getNode(macAddress);
				}
				else if(line.startsWith("ENERGY BY FRAME TYPE:"))
				{
					energy = true;
					dataSent = false;
					dataRecv = false;
				}
				else if(line.startsWith("DATA SENT BY FRAME TYPE:"))
				{
					energy = false;
					dataSent = true;
					dataRecv = false;
				}
				else if(line.startsWith("DATA RECEIVED BY FRAME TYPE:"))
				{
					energy = false;
					dataSent = false;
					dataRecv = true;
				}
				else
				{
					if(line.startsWith("MAC ADDR:"))
					{
						currentNode = currentSection.getNode(line.substring(11));
					}
					else if(line.startsWith("POSITION"))
					{
						split = line.substring(line.indexOf('X')+2).split(",");
						split[1] = split[1].substring(3);
						currentNode.setPosition(Integer.parseInt(split[0]), Integer.parseInt(split[1]));
					}
					else if(line.startsWith("TX POWER:"))
					{
						line = line.substring(11, line.length() - 3);
						if(line.equals("-inf"))
							currentNode.setTxPower(Math.pow(10, 0.1 * (Double.NEGATIVE_INFINITY - 30.0)));
						else
							currentNode.setTxPower(Math.pow(10, 0.1 * (Double.parseDouble(line) - 30.0)));
					}
					else if(line.startsWith("FINISH TIME:"))
					{
						currentNode.setFinishTime(Long.parseLong(line.substring(14, line.length() - 4)));
					}
					else if(line.startsWith("PARENT:"))
					{
						currentNode.setParent(currentSection.getNode(line.substring(10)));
					}
					else if(line.startsWith("FINISHED:"))
					{
						currentNode.setFinished(Boolean.parseBoolean(line.substring(11)));
					}
					else if(line.startsWith("MISSING APP DATA PACKETS:"))
					{
						currentNode.setMissingPackets(Integer.parseInt(line.substring(26)));
					}
					else if(line.startsWith("TOTAL NUMBER OF CYCLES:"))
					{
						currentNode.setCycles(Integer.parseInt(line.substring(24)));
					}
					else if(line.startsWith("\tCYCLE_CHECK:"))
					{
						if(energy)
						{
							line = line.substring(14);
							double[] d = Statistics.getEnergy(line);
							currentNode.setEnergyRecv(FRAME_TYPE.CYCLE_CHECK, d[0]);
							currentNode.setEnergySent(FRAME_TYPE.CYCLE_CHECK, d[1]);
						}
						else
						{
							line = line.substring(16);
							if(dataSent)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataSent(FRAME_TYPE.CYCLE_CHECK, d[0], d[1]);
							}
							else if(dataRecv)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataRecv(FRAME_TYPE.CYCLE_CHECK, d[0], d[1]);
							}
						}
					}
					else if(line.startsWith("\tNEIGHBOR_DISCOVERY:"))
					{
						if(energy)
						{
							line = line.substring(20);
							double[] d = Statistics.getEnergy(line);
							currentNode.setEnergyRecv(FRAME_TYPE.NEIGHBOR_DISCOVERY, d[0]);
							currentNode.setEnergySent(FRAME_TYPE.NEIGHBOR_DISCOVERY, d[1]);
						}
						else
						{
							line = line.substring(22);
							if(dataSent)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataSent(FRAME_TYPE.NEIGHBOR_DISCOVERY, d[0], d[1]);
							}
							else if(dataRecv)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataRecv(FRAME_TYPE.NEIGHBOR_DISCOVERY, d[0], d[1]);
							}
						}
					}
					else if(line.startsWith("\tCHILD_REQUEST:"))
					{
						if(energy)
						{
							line = line.substring(16);
							double[] d = Statistics.getEnergy(line);
							currentNode.setEnergyRecv(FRAME_TYPE.CHILD_REQUEST, d[0]);
							currentNode.setEnergySent(FRAME_TYPE.CHILD_REQUEST, d[1]);
						}
						else
						{
							line = line.substring(18);
							if(dataSent)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataSent(FRAME_TYPE.CHILD_REQUEST, d[0], d[1]);
							}
							else if(dataRecv)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataRecv(FRAME_TYPE.CHILD_REQUEST, d[0], d[1]);
							}
						}
					}
					else if(line.startsWith("\tCHILD_CONFIRMATION:"))
					{
						if(energy)
						{
							line = line.substring(20);
							double[] d = Statistics.getEnergy(line);
							currentNode.setEnergyRecv(FRAME_TYPE.CHILD_CONFIRMATION, d[0]);
							currentNode.setEnergySent(FRAME_TYPE.CHILD_CONFIRMATION, d[1]);
						}
						else
						{
							line = line.substring(22);
							if(dataSent)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataSent(FRAME_TYPE.CHILD_CONFIRMATION, d[0], d[1]);
							}
							else if(dataRecv)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataRecv(FRAME_TYPE.CHILD_CONFIRMATION, d[0], d[1]);
							}
						}
					}
					else if(line.startsWith("\tCHILD_REJECTION:"))
					{
						if(energy)
						{
							line = line.substring(17);
							double[] d = Statistics.getEnergy(line);
							currentNode.setEnergyRecv(FRAME_TYPE.CHILD_REJECTION, d[0]);
							currentNode.setEnergySent(FRAME_TYPE.CHILD_REJECTION, d[1]);
						}
						else
						{
							line = line.substring(19);
							if(dataSent)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataSent(FRAME_TYPE.CHILD_REJECTION, d[0], d[1]);
							}
							else if(dataRecv)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataRecv(FRAME_TYPE.CHILD_REJECTION, d[0], d[1]);
							}
						}
					}
					else if(line.startsWith("\tPARENT_REVOCATION:"))
					{
						if(energy)
						{
							line = line.substring(19);
							double[] d = Statistics.getEnergy(line);
							currentNode.setEnergyRecv(FRAME_TYPE.PARENT_REVOCATION, d[0]);
							currentNode.setEnergySent(FRAME_TYPE.PARENT_REVOCATION, d[1]);
						}
						else
						{
							line = line.substring(21);
							if(dataSent)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataSent(FRAME_TYPE.PARENT_REVOCATION, d[0], d[1]);
							}
							else if(dataRecv)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataRecv(FRAME_TYPE.PARENT_REVOCATION, d[0], d[1]);
							}
						}
					}
					else if(line.startsWith("\tEND_OF_GAME:"))
					{
						if(energy)
						{
							line = line.substring(14);
							double[] d = Statistics.getEnergy(line);
							currentNode.setEnergyRecv(FRAME_TYPE.END_OF_GAME, d[0]);
							currentNode.setEnergySent(FRAME_TYPE.END_OF_GAME, d[1]);
						}
						else
						{
							line = line.substring(16);
							if(dataSent)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataSent(FRAME_TYPE.END_OF_GAME, d[0], d[1]);
							}
							else if(dataRecv)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataRecv(FRAME_TYPE.END_OF_GAME, d[0], d[1]);
							}
						}
					}
					else if(line.startsWith("\tAPPLICATION_DATA:"))
					{
						if(energy)
						{
							line = line.substring(18);
							double[] d = Statistics.getEnergy(line);
							currentNode.setEnergyRecv(FRAME_TYPE.APPLICATION_DATA, d[0]);
							currentNode.setEnergySent(FRAME_TYPE.APPLICATION_DATA, d[1]);
						}
						else
						{
							line = line.substring(20);
							if(dataSent)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataSent(FRAME_TYPE.APPLICATION_DATA, d[0], d[1]);
							}
							else if(dataRecv)
							{
								int[] d = Statistics.getData(line);
								currentNode.setDataRecv(FRAME_TYPE.APPLICATION_DATA, d[0], d[1]);
							}
						}
					}
				}
			}
			
			
			for(Section section : sections)
			{
				section.finish();
				
				StringBuilder sb = new StringBuilder();
				sb.append(section.getTotalEnergy());
				sb.append(';');
				sb.append(section.getTotalTxPower());
				sb.append(';');
				sb.append(section.getTimeToBuild(false));
				sb.append(';');
				sb.append(section.getTimeToBuild(true));
				sb.append(';');
				sb.append(section.getTreeDepth());
				sb.append(';');
				sb.append(section.getUnconnectedNodes());
				sb.append(';');
				sb.append(section.getCycles());
				sb.append(';');
				sb.append(section.getCyclesLasted());
				sb.append(';');
				
				sb.append(section.getEnergyByFrameType(FRAME_TYPE.CYCLE_CHECK));
				sb.append(';');
				sb.append(section.getEnergyByFrameType(FRAME_TYPE.NEIGHBOR_DISCOVERY));
				sb.append(';');
				sb.append(section.getEnergyByFrameType(FRAME_TYPE.CHILD_REQUEST));
				sb.append(';');
				sb.append(section.getEnergyByFrameType(FRAME_TYPE.CHILD_CONFIRMATION));
				sb.append(';');
				sb.append(section.getEnergyByFrameType(FRAME_TYPE.CHILD_REJECTION));
				sb.append(';');
				sb.append(section.getEnergyByFrameType(FRAME_TYPE.PARENT_REVOCATION));
				sb.append(';');
				sb.append(section.getEnergyByFrameType(FRAME_TYPE.END_OF_GAME));
				sb.append(';');
				sb.append(section.getEnergyByFrameType(FRAME_TYPE.APPLICATION_DATA));
				sb.append(';');
				
				sb.append(section.getDataByFrameType(FRAME_TYPE.CYCLE_CHECK));
				sb.append(';');
				sb.append(section.getDataByFrameType(FRAME_TYPE.NEIGHBOR_DISCOVERY));
				sb.append(';');
				sb.append(section.getDataByFrameType(FRAME_TYPE.CHILD_REQUEST));
				sb.append(';');
				sb.append(section.getDataByFrameType(FRAME_TYPE.CHILD_CONFIRMATION));
				sb.append(';');
				sb.append(section.getDataByFrameType(FRAME_TYPE.CHILD_REJECTION));
				sb.append(';');
				sb.append(section.getDataByFrameType(FRAME_TYPE.PARENT_REVOCATION));
				sb.append(';');
				sb.append(section.getDataByFrameType(FRAME_TYPE.END_OF_GAME));
				sb.append(';');
				sb.append(section.getDataByFrameType(FRAME_TYPE.APPLICATION_DATA));
				sb.append(';');
				
				sb.append(section.getPacketsByFrameType(FRAME_TYPE.CYCLE_CHECK));
				sb.append(';');
				sb.append(section.getPacketsByFrameType(FRAME_TYPE.NEIGHBOR_DISCOVERY));
				sb.append(';');
				sb.append(section.getPacketsByFrameType(FRAME_TYPE.CHILD_REQUEST));
				sb.append(';');
				sb.append(section.getPacketsByFrameType(FRAME_TYPE.CHILD_CONFIRMATION));
				sb.append(';');
				sb.append(section.getPacketsByFrameType(FRAME_TYPE.CHILD_REJECTION));
				sb.append(';');
				sb.append(section.getPacketsByFrameType(FRAME_TYPE.PARENT_REVOCATION));
				sb.append(';');
				sb.append(section.getPacketsByFrameType(FRAME_TYPE.END_OF_GAME));
				sb.append(';');
				sb.append(section.getPacketsByFrameType(FRAME_TYPE.APPLICATION_DATA));
				sb.append(';');
				
				double[] loss = section.getPacketLossOnTreeDepth();
				for(double d : loss)
				{
					sb.append(d);
					sb.append(';');
				}
				
				System.out.println(sb.toString().replace('.', ','));
			}
		}
	}
	
	
	public static int getTreeDepth(String node, HashMap<String, ArrayList<String>> tree, int depth)
	{
		int d = depth;
		ArrayList<String> nodes = tree.remove(node);
		if(nodes != null)
		{
			for(String s : nodes)
			{
				int x = Statistics.getTreeDepth(s, tree, depth + 1);
				if(x > d)
					d = x;
			}
		}
		return d;
	}
	
	//0.00815916J | 0.17607J
	private static double[] getEnergy(String line)
	{
		double[] result = new double[2];
		
		String[] split = line.split("\\|");
		result[0] = Double.parseDouble(split[0].substring(0, split[0].length()-2));
		result[1] = Double.parseDouble(split[1].substring(1, split[1].length()-1));
		
		return result;
	}
	
	//0	 | 0B
	private static int[] getData(String line)
	{
		int[] result = new int[2];
		
		String[] split = line.split("\\|");
		result[0] = Integer.parseInt(split[0].substring(0, split[0].length() - 2));
		result[1] = Integer.parseInt(split[1].substring(1, split[1].length() - 1));
		
		return result;
	}
}