import com.fulldivegames.majorasmaskvr.UpdatePolicy;
public final class QuestUpdatePolicyTest {
 interface Action {void run()throws Exception;}
 static int checks;
 static void rejects(Action action)throws Exception{try{action.run();}catch(IllegalArgumentException expected){checks++;return;}throw new AssertionError("unsafe update accepted");}
 public static void main(String[] args)throws Exception{
  UpdatePolicy.url("https://example.com/releases/update.apk");checks++;
  for(String url:new String[]{"http://example.com/a","file:///tmp/a","https://user:password@example.com/a","https:/broken","/relative"})rejects(()->UpdatePolicy.url(url));
  UpdatePolicy.digest("a".repeat(64));checks++;
  for(String digest:new String[]{"a".repeat(63),"z".repeat(64),"",null})rejects(()->UpdatePolicy.digest(digest));
  UpdatePolicy.size(UpdatePolicy.MAX_APK_BYTES);checks++;
  for(long size:new long[]{0,-1,UpdatePolicy.MAX_APK_BYTES+1})rejects(()->UpdatePolicy.size(size));
  UpdatePolicy.identity("mmvr","mmvr",1,2);checks++;
  rejects(()->UpdatePolicy.identity("mmvr","other",1,2));
  rejects(()->UpdatePolicy.identity("mmvr","mmvr",2,1));
  rejects(()->UpdatePolicy.identity("mmvr","mmvr",2,2));
  rejects(()->UpdatePolicy.identity("mmvr","mmvr",2,(long)Integer.MAX_VALUE+1));
  System.out.println("Quest update policy: "+checks+" checks passed; Android installer/device confirmation remains a device test.");
 }
}
