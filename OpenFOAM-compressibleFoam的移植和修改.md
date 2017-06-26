---
title: OpenFOAM compressibleFoam的移植和修改
---

# 摘要

移植OpenFOAM的衍生代码compressibleFoam从OpenFOAM 2.4到OpenFOAM 4.1版本，采用git管理

# 建立git本地和远程分支

可以用如下命令：

```shell
# 设置本地commit所用的信息
git config --global user.name "Di Cheng"
git config --global user.email "chengdi123000@gmail.com"

# 注册账号，比如以chengdi123000@gmail.com注册账号
# 到https://github.com/pavanakumar/compressibleFoam右上角点fork
# 此时就会有一个https://github.com/chengdi123000/compressibleFoam生成
# 然后我们将其下载到本地
cd $HOME
mkdir oscfd
cd oscfd
git clone https://github.com/chengdi123000/compressibleFoam.git

# 配置一下，将上游的信息加入本地的备份。
git remote add upstream https://github.com/pavanakumar/compressibleFoam.git
git remote -v # 查看现有信息，大概应该如下所示
#origin	https://github.com/chengdi123000/compressibleFoam.git (fetch)
#origin	https://github.com/chengdi123000/compressibleFoam.git (push)
#upstream	https://github.com/pavanakumar/compressibleFoam.git (fetch)
#upstream	https://github.com/pavanakumar/compressibleFoam.git (push)

# 新建分支，名字是of41，表示移植到openfoam 4.1，假设此时已经安装好of41并且source了对应的bashrc
git checkout -b of41
# vim .gitignore可以选择不包含一些生成在当地的文件。比如Make文件夹下新生成的linux64GccDPInt32Opt文件夹
# 可以添加这么一行：Make/linux64GccDPInt32Opt/ 
# 从而匹配这个文件夹及其下属的所有文件被ignore掉。
# 做完移植，比如修改Make/files, Make/options等之后，用wmake测试编译通过
git add Make/files Make/options .gitignore # 加入所有的修改
git rm xxx #删除xxx
git mv xxx yyy #将文件xxx重命名为yyy
git commit -m "移植到openfoam 4.1，做了修改xxx,yyy,zzz"

# 此时就算修改完成了。可以考虑上传到github分享给大家
git push origin of41 #origin对应着https://github.com/chengdi123000/compressibleFoam.git，of41是刚才修改的branch的分支
# 此时需要输入github账户信息
# 如果经常提交，不想太麻烦可以设置ssh免密码提交，或者使用以下命令让git记住你的用户名和密码。
git config credential.helper store #这条命令会使用户名和密码加密储存在本地。
# 此时看到https://github.com/chengdi123000/compressibleFoam 网页上会出现`4 branches`字样，由于原来项目就有3个branches，of41就是新的第4个branch。
# 可以对比确认一下修改过得文件在master和of41下的异同。

# 下一步可以加入算例
# 拷贝算例
cp $FOAM_TUTORIALS/compressible/rhoCentralFoam/wedge15Ma5/ -r .
# 将这个文件夹加入stage中
git add wedge15Ma5/ #壁面将后面生成的文件也加入进来
# 修改wedge15Ma5并运行
cd wedge15Ma5
foamJob blockMesh
cat log #查看网格划分是否有错误
free -h #查看有多少内存可用，比较重要的是available，即新开始一个程序所能分配的最大内存量。
foamJob compressibleFoam_PM -mach 5 # 运行程序
touch a.foam
paraview a.foam
```

## OF中的args

```c++
//
// setRootCase.H
// ~~~~~~~~~~~~~

    Foam::argList args(argc, argv);
    if (!args.checkRootCase())
    {
        Foam::FatalError.exit();
    }
```

参考`refineMesh.C`中的示范，似乎openfoam程序需要先调用`argList::addOption("parameter_name","input_name","usage")`，再`include setRootCase.H`，随后就可以用`args.optionReadIfPresent("name", output_name)`来读取了。

```c++
// eulerSolver.C:25
//int main(int argc, char *argv[])
//{
//   #include "setRootCase.H"
//   #include "createTime.H"
//   #include "createMesh.H"
//   #include "setInputValues.H"
/*modified as :*/
int main(int argc, char *argv[])
{
   /// Register cmd line inputs
   Foam::argList::addOption( "mach", "Mach number" );
   Foam::argList::addOption( "aoa", "Angle of attack" );
   Foam::argList::addOption( "cfl", "CFL number" );
   #include "setRootCase.H"
   /// Default values
   scalar M_inf, aoa, CFL;
   if( args.optionReadIfPresent( "mach", M_inf ) == false )
     Info << "Need a Mach number input " << Foam::FatalError;
   /// read AOA
   if( args.optionReadIfPresent( "aoa", aoa ) == false ) aoa = 0.0;
   /// Read CFL
   if( args.optionReadIfPresent( "cfl", CFL ) == false ) CFL = 0.8;
   #include "createTime.H"
   #include "createMesh.H"

```

## OF中的runTime.run()和runTime.loop()

参考文档中的解释：

```c++
//- Return true if run should continue,
//  also invokes the functionObjectList::end() method
//  when the time goes out of range
//  \note
//  For correct behaviour, the following style of time-loop
//  is recommended:
//  \code
//      while (runTime.run())
//      {
//          runTime++;
//          solve;
//          runTime.write();
//      }
//  \endcode
virtual bool run() const;

//- Return true if run should continue and if so increment time
//  also invokes the functionObjectList::end() method
//  when the time goes out of range
//  \note
//  For correct behaviour, the following style of time-loop
//  is recommended:
//  \code
//      while (runTime.loop())
//      {
//          solve;
//          runTime.write();
//      }
//  \endcode
virtual bool loop();
```

简单说来，runTime.run()需要配合runTime++，runTime.loop()则不用。同时，runTime++似乎需要在solve之前调用。

# compressibleFoam 代码逻辑

绕开了OpenFOAM的大部分机制，只实现了显式的一阶Euler求解器。

- 初始化

  - 读取mach, aoa, cfl，似乎不再使用controlDict中设定的$\Delta t$
  - 初始化场：
    - 原始变量：rho, u, p
    - 守恒量：momFlux, massFlux, energyFlux
    - 残数：momResidue, massResidue, energyResidue
    - LTS时间：localDt
    - 表面单位向量场：nf

- 取得通量。

  - 利用face addressing，循环访问所有面。
    - 只循环owner，但是网格边界面有owner, 没neighbour。
  - 利用一阶重构，用左右单元的值计算通量，调用fluxSolver
  - 同时计算localDt

- 通量求和得Residue

  - momResidue, massResidue, energyResidue

- 计算边界的贡献

  - 对所有Boundary循环

    - ```c++
      scalar bflux[5];
      word BCTypePhysical = mesh.boundaryMesh().physicalTypes()[ipatch];
      word BCType         = mesh.boundaryMesh().types()[ipatch];
      word BCName         = mesh.boundaryMesh().names()[ipatch];
      const UList<label> &bfaceCells = mesh.boundaryMesh()[ipatch].faceCells();
      ```

    - 手动分类处理边界。

      - BCTypePhysical
        - slip=symmetry
        - extrapolatedOutflow
        - riemannExtrapolation
        - supersonicInlet
        - 从代码来看，只要在`constant/polyMesh/boundary`中直接指定physicalType为上述几个类型就可以了。在`0`文件夹中似乎都不需要边界条件。
      - BCType
        - processor

    - 因为没有用到fvMatrix类。

  - 时间递进，求解下一时间步的状态。

    - 计算全局最大最小特征值
    - CFL数对localDt进行缩放
    - 计算下一时间步状态

  - runTime.write()

