close all
sCmd = 'LD_LIBRARY_PATH=../libtascar/build/:../plugins/build ../apps/build/tascar_renderfile';
name = 'test_pos_door';
system(sprintf('%s -i zeros.wav -d -f 64 -o %s.wav %s.tsc',sCmd,name,name));
d = audioread([name,'.wav']);
d = d(1:64:end,:);
len = size(d,1);
hold off
r = -d(:,1:3);
r(:,1) = r(:,1) - 1;
d = d(:,5:7) - d(:,1:3);
d(:,1) = d(:,1) - 1;
plot(d(:,1:4:end),d(:,2:4:end),'.');
hold on
plot(r(:,1:4:end),r(:,2:4:end),'.');
for k=0:4
    l = 1+floor((len-1)*k/4);
    plot(d(l,1:4:end),d(l,2:4:end),'ko');
    text(d(l,1:4:end),d(l,2:4:end),sprintf(' %d',k));
    plot(r(l,1:4:end),r(l,2:4:end),'ko');
    text(r(l,1:4:end),r(l,2:4:end),sprintf(' %d',k));
end
set(gca,'DataAspectRatio',[1,1,1]);
hold on;
plot([-1,-1],[-0.5,0.5],'k-+');