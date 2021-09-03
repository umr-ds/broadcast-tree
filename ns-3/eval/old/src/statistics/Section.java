package statistics;

import java.util.ArrayList;
import java.util.HashMap;

import statistics.Statistics.FRAME_TYPE;

public class Section
{
	private Node source;
	private HashMap<String, Node> nodes;
	private HashMap<Node, ArrayList<Node>> childs;
	
	private double[] energy;
	private double totalEnergy, totalTxPower;
	private int[][] data_sent, data_recv, lostPackets;
	private int treeDepth, unconNodes, cycles, lastedCycles;
	
	public Section()
	{
		this.nodes = new HashMap<>();
		this.childs = new HashMap<>();
		
		this.data_sent = new int[8][2];
		this.data_recv = new int[8][2];
		this.energy = new double[8];
	}
	
	/*
	 * Nodes
	 */
	public void addNode(Node n)
	{
		this.nodes.put(n.getMacAddress(), n);
		this.childs.put(n, new ArrayList<>());
	}
	
	public Node getNode(String macAddress)
	{
		return this.nodes.get(macAddress);
	}
	
	public void setSource(Node node)
	{
		this.source = node;
	}
	
	
	public void finish()
	{
		for(Node node : this.nodes.values())
		{
			if(node.getMacAddress().equals("ff:ff:ff:ff:ff:ff"))
				continue;
			//Add node to child list of parent
			this.childs.get(this.nodes.get(node.getParent().getMacAddress())).add(node);
		}
		
		HashMap<Node, ArrayList<Node>> cL = (HashMap<Node, ArrayList<Node>>)this.childs.clone();
		this.findCycles(this.source, cL);
		this.unconNodes = cL.size();
		//System.out.println("================");
		while(cL.size() > 0)
		{
			Node r = null;
			for(Node n : cL.keySet())
			{
				r = n;
				break;
			}
			
			if(r.getMacAddress().equals("ff:ff:ff:ff:ff:ff"))
				cL.remove(r);
			else if(this.findCycles(r, cL))
			{
				System.out.println("Cycle for " + r.getMacAddress());
				this.lastedCycles++;
			}
		}
		
		this.treeDepth = this.buildTree(this.source, 0);
		
		this.lostPackets = new int[this.treeDepth + 1][2];
		
		for(Node node : this.nodes.values())
		{
			if(node.getMacAddress().equals("ff:ff:ff:ff:ff:ff"))
				continue;
			
			this.totalEnergy += node.getEnergyRecv();
			this.totalEnergy += node.getEnergySend();
			
			this.totalTxPower += node.getTxPower();
			
			this.cycles += node.getCycles();
			
			this.lostPackets[node.getTreeDepth()][0]++;
			this.lostPackets[node.getTreeDepth()][1] += node.getMissingPackets();
			
			this.energy[0] += node.getEnergyRecv(FRAME_TYPE.CYCLE_CHECK);
			this.energy[1] += node.getEnergyRecv(FRAME_TYPE.NEIGHBOR_DISCOVERY);
			this.energy[2] += node.getEnergyRecv(FRAME_TYPE.CHILD_REQUEST);
			this.energy[3] += node.getEnergyRecv(FRAME_TYPE.CHILD_CONFIRMATION);
			this.energy[4] += node.getEnergyRecv(FRAME_TYPE.CHILD_REJECTION);
			this.energy[5] += node.getEnergyRecv(FRAME_TYPE.PARENT_REVOCATION);
			this.energy[6] += node.getEnergyRecv(FRAME_TYPE.END_OF_GAME);
			this.energy[7] += node.getEnergyRecv(FRAME_TYPE.APPLICATION_DATA);
			
			this.energy[0] += node.getEnergySent(FRAME_TYPE.CYCLE_CHECK);
			this.energy[1] += node.getEnergySent(FRAME_TYPE.NEIGHBOR_DISCOVERY);
			this.energy[2] += node.getEnergySent(FRAME_TYPE.CHILD_REQUEST);
			this.energy[3] += node.getEnergySent(FRAME_TYPE.CHILD_CONFIRMATION);
			this.energy[4] += node.getEnergySent(FRAME_TYPE.CHILD_REJECTION);
			this.energy[5] += node.getEnergySent(FRAME_TYPE.PARENT_REVOCATION);
			this.energy[6] += node.getEnergySent(FRAME_TYPE.END_OF_GAME);
			this.energy[7] += node.getEnergySent(FRAME_TYPE.APPLICATION_DATA);
			
			this.data_recv[0][0] += node.getDataRecv(FRAME_TYPE.CYCLE_CHECK);
			this.data_recv[1][0] += node.getDataRecv(FRAME_TYPE.NEIGHBOR_DISCOVERY);
			this.data_recv[2][0] += node.getDataRecv(FRAME_TYPE.CHILD_REQUEST);
			this.data_recv[3][0] += node.getDataRecv(FRAME_TYPE.CHILD_CONFIRMATION);
			this.data_recv[4][0] += node.getDataRecv(FRAME_TYPE.CHILD_REJECTION);
			this.data_recv[5][0] += node.getDataRecv(FRAME_TYPE.PARENT_REVOCATION);
			this.data_recv[6][0] += node.getDataRecv(FRAME_TYPE.END_OF_GAME);
			this.data_recv[7][0] += node.getDataRecv(FRAME_TYPE.APPLICATION_DATA);
			
			this.data_recv[0][1] += node.getPacketsRecv(FRAME_TYPE.CYCLE_CHECK);
			this.data_recv[1][1] += node.getPacketsRecv(FRAME_TYPE.NEIGHBOR_DISCOVERY);
			this.data_recv[2][1] += node.getPacketsRecv(FRAME_TYPE.CHILD_REQUEST);
			this.data_recv[3][1] += node.getPacketsRecv(FRAME_TYPE.CHILD_CONFIRMATION);
			this.data_recv[4][1] += node.getPacketsRecv(FRAME_TYPE.CHILD_REJECTION);
			this.data_recv[5][1] += node.getPacketsRecv(FRAME_TYPE.PARENT_REVOCATION);
			this.data_recv[6][1] += node.getPacketsRecv(FRAME_TYPE.END_OF_GAME);
			this.data_recv[7][1] += node.getPacketsRecv(FRAME_TYPE.APPLICATION_DATA);
			
			this.data_sent[0][0] += node.getDataSent(FRAME_TYPE.CYCLE_CHECK);
			this.data_sent[1][0] += node.getDataSent(FRAME_TYPE.NEIGHBOR_DISCOVERY);
			this.data_sent[2][0] += node.getDataSent(FRAME_TYPE.CHILD_REQUEST);
			this.data_sent[3][0] += node.getDataSent(FRAME_TYPE.CHILD_CONFIRMATION);
			this.data_sent[4][0] += node.getDataSent(FRAME_TYPE.CHILD_REJECTION);
			this.data_sent[5][0] += node.getDataSent(FRAME_TYPE.PARENT_REVOCATION);
			this.data_sent[6][0] += node.getDataSent(FRAME_TYPE.END_OF_GAME);
			this.data_sent[7][0] += node.getDataSent(FRAME_TYPE.APPLICATION_DATA);
			
			this.data_sent[0][1] += node.getPacketsSent(FRAME_TYPE.CYCLE_CHECK);
			this.data_sent[1][1] += node.getPacketsSent(FRAME_TYPE.NEIGHBOR_DISCOVERY);
			this.data_sent[2][1] += node.getPacketsSent(FRAME_TYPE.CHILD_REQUEST);
			this.data_sent[3][1] += node.getPacketsSent(FRAME_TYPE.CHILD_CONFIRMATION);
			this.data_sent[4][1] += node.getPacketsSent(FRAME_TYPE.CHILD_REJECTION);
			this.data_sent[5][1] += node.getPacketsSent(FRAME_TYPE.PARENT_REVOCATION);
			this.data_sent[6][1] += node.getPacketsSent(FRAME_TYPE.END_OF_GAME);
			this.data_sent[7][1] += node.getPacketsSent(FRAME_TYPE.APPLICATION_DATA);
		}
	}
	
	
	/*
	 * Statistics
	 */
	public long getTimeToBuild(boolean useLastNode)
	{
		long time = 0;
		if(useLastNode)
		{
			for(Node n : this.nodes.values())
				if(time < n.getFinishTime())
					time = n.getFinishTime();
		}
		else
			time = this.source.getFinishTime();
		return time;
	}
	
	public double getTotalEnergy()
	{
		return this.totalEnergy;
	}
	
	public double getEnergyByFrameType(FRAME_TYPE ft)
	{
		return this.energy[ft.ordinal()];
	}
	
	public int getDataByFrameType(FRAME_TYPE ft)
	{
		return this.data_sent[ft.ordinal()][0];
	}
	
	public int getPacketsByFrameType(FRAME_TYPE ft)
	{
		return this.data_sent[ft.ordinal()][1];
	}
	
	public double getTotalTxPower()
	{
		return this.totalTxPower;
	}
	
	public int getTreeDepth()
	{
		return this.treeDepth;
	}
	
	public int getUnconnectedNodes()
	{
		return this.unconNodes;
	}
	
	public int getCycles()
	{
		return this.cycles;
	}
	
	public int getCyclesLasted()
	{
		return this.lastedCycles;
	}
	
	public double[] getPacketLossOnTreeDepth()
	{
		double[] result = new double[this.lostPackets.length];
		for(int i = 0; i < this.lostPackets.length; i++)
		{
			result[i] = ( ((double)this.lostPackets[i][1]) / (this.lostPackets[i][0] * 1000));
		}
		return result;
	}
	
	
	/*
	 * 
	 */
	private int buildTree(Node node, int depth)
	{
		node.setTreeDepth(depth);
		ArrayList<Node> childnodes = this.childs.remove(node);
		
		/*for(int i = 0; i < depth; i++)
			System.out.print('\t');
		System.out.println("Child nodes of [" + node.getMacAddress() + "]:");*/
		
		int d = depth;
		for(Node n : childnodes)
		{
			int x = this.buildTree(n, depth + 1);
			if(x > d)
				d = x;
		}
		
		this.childs.put(node, childnodes);
		return d;
	}
	
	private boolean findCycles(Node root, HashMap<Node, ArrayList<Node>> childList)
	{
		//System.out.println("findCycles(" + root.getMacAddress() + ",...)");
		ArrayList<Node> childnodes = childList.remove(root);
		if(childnodes != null)
		{
			boolean b = false;
			for(Node n : childnodes)
			{
				if(this.findCycles(n, childList))
					b = true;
			}
			
			return b;
		}
		else
		{
			return true;
		}
	}
}