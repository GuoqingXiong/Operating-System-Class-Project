/**
 * Created by guoqingxiong on 12/6/15.
 */
import java.io.*;
import java.util.Enumeration;
import java.util.Hashtable;

public class MemoryManagement {             //When page fault of PFF and VSWS are the same,minimum number of frames  and maximum number of frames of VSWS is smaller
                                            //Toal number of Page Fault of PFF is smaller than VSWS
                                            //VSWS can reduce the peak memory demands caused by abrupt inter-locality transitions

    public void PFF(String s)throws Exception{
        FileReader file = new FileReader(s);
        BufferedReader reader = new BufferedReader(file);
        String address = reader.readLine();
        String PageNmuber=address;
        address=reader.readLine();
        Hashtable<String,Integer> RS=new Hashtable<>();
        int F=50;                              //when F is small(from 1-10) and gets bigger, page fault will be smaller fast;
                                                //when F is bigger than 15, page fault will get smaller slowly;
                                                //when F is big enough, page fault comes to the minimum and will not change;
                                                //when F is bigger, minimum number of frames  and maximum number of frames will get bigger
        int PageFaultCount=0;
        int start=0;
        int index=0;
        int max=0;
        int min=Integer.MAX_VALUE;
        while (address!=null){
            if (!RS.containsKey(address)){
                PageFaultCount++;
                RS.put(address,1);
                if (index-start>=F){
                    Enumeration<String> enumeration=RS.keys();
                    while (enumeration.hasMoreElements()){
                        String pageReference=enumeration.nextElement();
                        if (RS.get(pageReference)==0){
                            RS.remove(pageReference);
                        }else{
                            RS.put(pageReference,0);
                        }
                    }
                    min=RS.size()<min ? RS.size() : min;
                }
                start=index;
            }else{
                RS.put(address,1);
            }
            max=RS.size()>max ? RS.size() : max;
            address=reader.readLine();
            index++;
        }
        System.out.println("PageFaultNumber= " + PageFaultCount);
        System.out.println("Min= " + min);
        System.out.println("Max= " + max);
    }

    public void VSWS(String s) throws Exception{
        FileReader file = new FileReader(s);
        BufferedReader reader = new BufferedReader(file);
        String address = reader.readLine();
        String PageNmuber=address;
        address=reader.readLine();
        Hashtable<String,Integer> RS=new Hashtable<>();
        int M=200;
        int L=300;
        int Q=20;
                                    //when M gets bigger, page fault first will not change, then will be smaller
                                    //when L gets bigger, page fault will be smaller first,then will not change when L gets enough big
                                    //when Q gets bigger, page fault will be smaller first,then will not change when Q gets enough big
        int PageFaultCount=0;
        int index=0;
        int periodStart=0;
        int thePF=0;
        int max=0;
        int min=Integer.MAX_VALUE;
        while (address!=null){
            if (!RS.containsKey(address)){
                PageFaultCount++;
                RS.put(address,1);
                thePF++;
            }else{
                RS.put(address,1);
            }
            if (index-periodStart>=M && thePF>=Q || index-periodStart>=L){
                Enumeration<String> enumeration=RS.keys();
                while (enumeration.hasMoreElements()){
                    String pageReference=enumeration.nextElement();
                    if (RS.get(pageReference)==0){
                        RS.remove(pageReference);
                    }else{
                        RS.put(pageReference,0);
                    }
                }
                periodStart=index;
                thePF=0;
                min=RS.size()<min ? RS.size() : min;
            }
            max=RS.size()>max ? RS.size() : max;
            address=reader.readLine();
            index++;
        }
        System.out.println("PageFaultNumber= " + PageFaultCount);
        System.out.println("Min= " + min);
        System.out.println("Max= " + max);
    }



    public void generatePageReferences(String s) throws Exception{          //generate Page References using principle of locality and transition situation
        FileWriter fw = new FileWriter(s);
        BufferedWriter bw = new BufferedWriter(fw);
        int Pages=100;
        int e=15;
        int p=0;
        int m=50;
        double t=0.2;
        for (int i = 0; i <120; i++) {
            for (int j = 0; j < m; j++) {
                int ran = p + (int) (Math.random() * e);
                if (ran>Pages-1){
                    continue;
                }
                bw.write(String.valueOf(ran));
                bw.newLine();
            }
            double r=Math.random();
            if (r<t){
                p=(int)(Math.random()*Pages);
            }else {
                p++;
            }
        }
        bw.flush();
        bw.close();
        fw.close();
    }



    public static void main(String[] args) throws Exception{
        String s=args[0]+".txt";
        MemoryManagement test=new MemoryManagement();
        test.generatePageReferences(s);         //Page references generated by myself, commented out this line when using another data set
        test.PFF(s);
        test.VSWS(s);
    }

}
